#include "serialcontroller.h"

#include <QDir>

// 构造函数：把底层串口的两个关键信号接到本类的私有槽上。
// 这样外部只需要监听我们对外暴露的三个信号即可。
SerialController::SerialController(QObject *parent) : QObject(parent) {
  connect(&m_serial, &QSerialPort::readyRead, this,
          &SerialController::handleReadyRead);
  connect(&m_serial, &QSerialPort::errorOccurred, this,
          &SerialController::handleError);
}

// 静态方法：扫描 /dev 目录，只列出真实的串口设备。
// 只匹配 ttyS*(板载串口) / ttyUSB*(USB转串口) / ttyACM*(CDC/ACM)，避免把
// tty、pts 等非串口设备也列出来。返回的是短名，如 "ttyACM0"。
QStringList SerialController::availablePorts() {
  QDir devDir("/dev");
  QStringList filters;
  filters << "ttyS*" << "ttyUSB*" << "ttyACM*";
  QStringList names =
      devDir.entryList(filters, QDir::System | QDir::Files | QDir::Readable);
  names.sort();
  return names;
}

bool SerialController::open(const QString &portName, int baudRate) {
  // 若已打开先关掉，保证状态干净。
  if (m_serial.isOpen())
    m_serial.close();

  // Linux 下允许用户只填 "ttyACM0"，这里补全为 "/dev/ttyACM0"。
  // Windows 下端口名形如 "COM3"，本身以字母开头，不会被误加前缀。
  QString name = portName.trimmed();
  if (!name.isEmpty() && !name.startsWith('/') && name.startsWith("tty"))
    name.prepend("/dev/");

  // 串口参数：8 位数据位、无校验、1 位停止位、无流控（即常说的 8N1）。
  m_serial.setPortName(name);
  m_serial.setBaudRate(baudRate);
  m_serial.setDataBits(QSerialPort::Data8);
  m_serial.setParity(QSerialPort::NoParity);
  m_serial.setStopBits(QSerialPort::OneStop);
  m_serial.setFlowControl(QSerialPort::NoFlowControl);

  const bool ok = m_serial.open(QIODevice::ReadWrite);
  if (ok)
    emit connectionChanged(true); // 通知外界：已打开
  return ok;                      // 失败原因由调用方通过 lastError() 获取
}

void SerialController::close() {
  if (m_serial.isOpen()) {
    m_serial.close();
    emit connectionChanged(false); // 通知外界：已关闭
  }
}

bool SerialController::isOpen() const { return m_serial.isOpen(); }

qint64 SerialController::write(const QByteArray &data) {
  return m_serial.write(data);
}

QString SerialController::lastError() const { return m_serial.errorString(); }

// 私有槽：把一次可读到的数据整包读出来，通过信号抛给外界处理。
void SerialController::handleReadyRead() {
  const QByteArray data = m_serial.readAll();
  if (!data.isEmpty())
    emit dataReceived(data);
}

// 私有槽：统一处理底层错误。
// 打开阶段的错误（如设备不存在、无权限）由 open() 的返回值处理，这里不再重复提示；
// 但连接中途掉线（ResourceError）需要主动关闭并告知界面。
void SerialController::handleError(QSerialPort::SerialPortError error) {
  if (error == QSerialPort::NoError)
    return;

  if (error == QSerialPort::ResourceError) {
    const QString message = m_serial.errorString();
    close();
    emit errorOccurred(QStringLiteral("串口连接中断：") + message);
    return;
  }

  // 其他错误若发生在已打开状态，则如实上报。
  if (m_serial.isOpen())
    emit errorOccurred(m_serial.errorString());
}
