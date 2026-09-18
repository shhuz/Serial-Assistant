#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QMainWindow>

class QTimer;          // 前向声明，减少头文件依赖
class SerialController;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief 主窗口：只负责「界面交互」与「显示」，
 *        串口通信细节全部委托给 SerialController。
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  // 以下四个槽名遵循 Qt 的自动连接约定 on_<对象名>_clicked，
  // 由 ui->setupUi() 生成的代码自动连接，无需手写 connect。
  void on_btnOpen_clicked();    // 打开/关闭串口
  void on_btnSend_clicked();    // 发送数据
  void on_btnClear_clicked();   // 清空接收区
  void on_btnRefresh_clicked(); // 手动刷新串口列表
  void on_checkHexSend_toggled(bool checked); // HEX 发送：切换发送框的显示形式
  void on_editSend_textChanged();             // 同步发送框内容到 m_sendData

  // 以下三个槽连接到 SerialController 的信号
  void onSerialData(const QByteArray &data);  // 收到数据
  void onSerialError(const QString &message); // 串口错误
  void onConnectionChanged(bool open);        // 连接状态变化

private:
  Ui::MainWindow *ui;         // 由 .ui 文件生成界面对象
  SerialController *serial;   // 串口控制器
  QTimer *portScanTimer;      // 定时扫描串口（热插拔）
  QTimer *rxIdleTimer;        // 接收空闲计时：超时后自动换行
  bool m_rxLineOpen = false;  // 接收行是否仍处于「同一行」的连续状态
  QByteArray m_sendData;      // 实际要发送的字节（发送框的逻辑内容）
  bool m_updatingSend = false; // 防止显示切换与内容同步相互触发

  void refreshPorts(); // 重新枚举可用串口并更新下拉框
  bool isAtBottom() const;
  void scrollToBottom();
  QString formatForDisplay(const QByteArray &data) const; // 按 HEX/文本格式化
  QByteArray buildPayload() const;                        // 组装待发送字节
  void appendRecord(const QString &tag, const QString &color,
                    const QString &content); // 追加一条收发记录
  void appendNotice(const QString &msg,
                    const QString &color = "#95a5a6"); // 追加一条提示
};

#endif // MAINWINDOW_H
