#include "serialcontroller.h"

#include <QDir>
#include <QFile>
#include <QSerialPortInfo>

// 构造函数：把底层串口的两个关键信号接到本类的私有槽上。
// 这样外部只需要监听我们对外暴露的三个信号即可。
SerialController::SerialController(QObject *parent) : QObject(parent) {
  connect(&m_serial, &QSerialPort::readyRead, this,
          &SerialController::handleReadyRead);
  connect(&m_serial, &QSerialPort::errorOccurred, this,
          &SerialController::handleError);
}

// 静态方法：枚举可用串口，返回设备名（Linux "ttyUSB0" / Windows "COM3" / macOS "cu.usbserial-*"）。
QStringList SerialController::availablePorts() {
  QStringList names;

  // 1) 主力：Qt 官方的跨平台枚举，只列内核认得的真串口，
  //    还能顺带拿到描述、VID/PID（info.description() 等，界面里可用来显示厂商）。
  for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
#ifdef Q_OS_LINUX
    // 下面两条都是 Linux 特有的「幽灵端口」过滤（其它平台不需要，就不编进去）：

    // 1a) /sys 设备树里可能登记着主机上并没有的端口，列出来也打不开 → 按节点是否真实存在过滤
    if (!QFile::exists(info.systemLocation()))
      continue;

    // 1b) 内核 8250 驱动会注册 32 个 legacy 串口（ttyS0~ttyS31），主板上往往根本没有对应硬件，
    //     它们也没有 USB 属性。判据和 Arduino 生态一致：真正的 USB 串口在 sysfs 里属于
    //     usb / usb-serial 子系统，在 Qt 这边就体现为有 VID/PID（hasVendorIdentifier）。
    //     真要用板载串口，手动输入 ttyS0 即可（下拉框本就可编辑）。
    if (info.portName().startsWith(QStringLiteral("ttyS")) &&
        !info.hasVendorIdentifier())
      continue;
#endif
    names << info.portName();
  }

#ifdef Q_OS_LINUX
  // 2) 兜底（仅 Linux）：socat 之类造的虚拟串口只是 /dev 下的软链接，不在 /sys 里，
  //    QSerialPortInfo 看不到，但从目录能扫到 —— 调试与自测都靠它。
  //    这里刻意不含 ttyS*：那是 1b) 已经排除掉的 8250 占位端口。
  const QDir devDir(QStringLiteral("/dev"));
  const QStringList filters{"ttyUSB*", "ttyACM*"};
  names << devDir.entryList(filters, QDir::System | QDir::Files | QDir::Readable);
#endif

  names.removeDuplicates();
  names.sort();
  return names;
}

bool SerialController::open(const QString &portName, int baudRate) {
  // 若已打开先关掉，保证状态干净。
  if (m_serial.isOpen())
    m_serial.close();

  // Unix 上把短名补成完整路径：Linux "ttyUSB0" -> "/dev/ttyUSB0"，
  // macOS "cu.usbserial-XXXX" -> "/dev/cu.usbserial-XXXX"；已经带路径的保持原样。
  // Windows 的 "COM3" 本身就是完整名字，不能加前缀。
  QString name = portName.trimmed();
#ifndef Q_OS_WIN
  if (!name.isEmpty() && !name.startsWith(QLatin1Char('/')))
    name.prepend(QStringLiteral("/dev/"));
#endif

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
