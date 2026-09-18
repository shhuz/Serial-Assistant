#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QStringList>

/**
 * @brief 串口通信封装类。
 *
 * 设计目的：把 QSerialPort 的底层细节（配置、读写、错误码）隐藏在本类内部，
 * 只用「信号」向外界汇报三件事：收到数据、发生错误、连接状态变化。
 * 这样子界面（MainWindow）只关心业务，不用直接操作 QSerialPort，
 * 便于维护与单元测试 —— 这就是所谓的「关注点分离 / 分层」。
 */
class SerialController : public QObject {
  Q_OBJECT

public:
  explicit SerialController(QObject *parent = nullptr);

  /// 扫描 /dev 列出真实串口设备的短名（如 "ttyACM0"），已排序。
  /// 仅匹配 ttyS* / ttyUSB* / ttyACM*，不会列出 tty、pts 等非串口设备。
  static QStringList availablePorts();

  /// 打开串口。portName 可传短名或完整路径；baudRate 为波特率。
  /// @return 打开成功返回 true，失败返回 false（可用 lastError() 查看原因）。
  bool open(const QString &portName, int baudRate);

  /// 关闭串口（若已打开会发出 connectionChanged(false)）。
  void close();

  /// 串口当前是否处于打开状态。
  bool isOpen() const;

  /// 向串口写入数据，返回实际写入的字节数。
  qint64 write(const QByteArray &data);

  /// 最近一次操作的错误描述（用于界面提示）。
  QString lastError() const;

signals:
  void dataReceived(const QByteArray &data);  ///< 收到新数据
  void errorOccurred(const QString &message); ///< 通信过程中发生错误
  void connectionChanged(bool open);          ///< 串口被打开(true)/关闭(false)

private slots:
  void handleReadyRead();                              ///< 内部：有数据可读
  void handleError(QSerialPort::SerialPortError error); ///< 内部：错误回调

private:
  QSerialPort m_serial; ///< 真正干活的 Qt 串口对象
};

#endif // SERIALCONTROLLER_H
