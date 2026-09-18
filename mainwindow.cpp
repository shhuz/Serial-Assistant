#include "mainwindow.h"
#include "serialcontroller.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QKeySequence>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCursor>
#include <QTimer>

namespace {
// —— 日志配色（与样式表中的深色接收区搭配）——
const char *kColorRx = "#2ecc71";    // 接收：绿色
const char *kColorTx = "#3498db";    // 发送：蓝色
const char *kColorInfo = "#95a5a6";  // 提示：灰色
const char *kColorError = "#e74c3c"; // 错误：红色

// 串口自动扫描间隔（毫秒），用于实现热插拔刷新
const int kPortScanIntervalMs = 2000;

// 把接收到的文本转成可插入接收区的 HTML：
// 先统一换行符，再去掉末尾换行（避免末尾多出一行空白），最后把换行转成 <br>。
QString contentToHtml(const QString &text) {
  QString t = text;
  t.replace("\r\n", "\n");
  t.replace('\r', '\n');
  if (t.endsWith('\n'))
    t.chop(1);
  QString html = t.toHtmlEscaped();
  html.replace('\n', "<br>");
  return html;
}

// 判断原始数据是否以换行符结尾——决定下一包数据是否另起一行。
bool endsWithLineBreak(const QByteArray &data) {
  return !data.isEmpty() && (data.back() == '\n' || data.back() == '\r');
}

// 只保留 0-9 / A-F（一律大写），其它字符（g、v、空格、逗号等）全部丢弃。
QString hexDigitsOnly(const QString &text) {
  QString digits;
  digits.reserve(text.size());
  for (const QChar &ch : text) {
    const QChar up = ch.toUpper();
    if ((up >= '0' && up <= '9') || (up >= 'A' && up <= 'F'))
      digits.append(up);
  }
  return digits;
}

// 规范化十六进制输入：大写、每两位一组、用空格分隔（如 "74747474" -> "74 74 74 74"）。
QString normalizeHexInput(const QString &text) {
  const QString digits = hexDigitsOnly(text);
  QString out;
  for (int i = 0; i < digits.size(); i += 2) {
    if (!out.isEmpty())
      out.append(' ');
    out.append(digits.mid(i, 2));
  }
  return out;
}

// 十六进制文本 -> 字节；忽略非十六进制字符，未成对的半字节忽略。
QByteArray hexTextToBytes(const QString &text) {
  QString digits = hexDigitsOnly(text);
  if (digits.size() % 2 != 0)
    digits.chop(1);
  return QByteArray::fromHex(digits.toLatin1());
}

// 字节 -> 十六进制显示文本（如 "41 42"）。
QString bytesToHexText(const QByteArray &bytes) {
  return QString::fromLatin1(bytes.toHex(' ')).toUpper();
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      serial(new SerialController(this)), portScanTimer(new QTimer(this)) {
  ui->setupUi(this);

  // 1) 波特率：给出常用预设，并允许自定义输入。
  ui->comboBaud->addItems({"9600", "19200", "38400", "57600", "115200"});
  ui->comboBaud->setCurrentText("115200");
  ui->comboBaud->setEditable(true);

  // 2) 端口下拉框允许手动输入（自动扫描只做补充）。
  ui->comboPort->setEditable(true);

  // 3) 行尾符：显示文本放进 itemText，真正要发送的字节放进 itemData。
  ui->comboLineEnd->addItem("无", QByteArray());
  ui->comboLineEnd->addItem("\\n", QByteArray("\n"));
  ui->comboLineEnd->addItem("\\r\\n", QByteArray("\r\n"));
  ui->comboLineEnd->setCurrentIndex(1);

  // 4) 首次枚举串口。
  refreshPorts();

  // 5) 定时扫描实现热插拔：插上/拔掉 USB 串口后列表会自动更新。
  portScanTimer->setInterval(kPortScanIntervalMs);
  connect(portScanTimer, &QTimer::timeout, this, &MainWindow::refreshPorts);
  portScanTimer->start();

  // 6) 把串口控制器的信号接到本窗口的槽。
  connect(serial, &SerialController::dataReceived, this, &MainWindow::onSerialData);
  connect(serial, &SerialController::errorOccurred, this, &MainWindow::onSerialError);
  connect(serial, &SerialController::connectionChanged, this,
          &MainWindow::onConnectionChanged);

  // 7) 接收空闲计时器：数据片段间隔超过设定毫秒数后，认为一条消息结束，
  //    下一段数据自动另起一行（即“空闲换行”）。
  rxIdleTimer = new QTimer(this);
  rxIdleTimer->setSingleShot(true);
  connect(rxIdleTimer, &QTimer::timeout, this, [this] { m_rxLineOpen = false; });

  // 8) 快捷键 Ctrl+Enter 发送（发送框是多行文本框，回车用于换行）。
  auto *sendShortcut = new QShortcut(QKeySequence("Ctrl+Return"), this);
  connect(sendShortcut, &QShortcut::activated, this, &MainWindow::on_btnSend_clicked);

  appendNotice("就绪：请选择串口并点击“打开串口”");
}

MainWindow::~MainWindow() {
  // 销毁前断开串口信号，避免窗口析构过程中被回调。
  serial->disconnect(this);
  delete ui;
}

// 重新枚举可用串口并同步到下拉框。
void MainWindow::refreshPorts() {
  // 串口已打开时不改动列表，避免干扰当前选择。
  if (serial->isOpen())
    return;

  const QStringList ports = SerialController::availablePorts();

  // 列表内容未变化就直接返回，避免下拉框反复重建导致闪烁。
  QStringList current;
  for (int i = 0; i < ui->comboPort->count(); ++i)
    current << ui->comboPort->itemText(i);
  if (current == ports)
    return;

  // 记录当前选择，重建后尽量恢复。
  const QString selected = ui->comboPort->currentText();
  ui->comboPort->clear();
  ui->comboPort->addItems(ports);
  if (!selected.isEmpty() && ports.contains(selected))
    ui->comboPort->setCurrentText(selected);
}

void MainWindow::on_btnRefresh_clicked() { refreshPorts(); }

void MainWindow::on_btnOpen_clicked() {
  // 已打开 -> 关闭（界面状态在 onConnectionChanged 中统一更新）。
  if (serial->isOpen()) {
    serial->close();
    return;
  }

  // 未打开 -> 校验输入并尝试打开。
  const QString port = ui->comboPort->currentText().trimmed();
  if (port.isEmpty()) {
    appendNotice("请先选择一个串口", kColorError);
    return;
  }
  const int baud = ui->comboBaud->currentText().toInt();
  if (!serial->open(port, baud))
    appendNotice("打开失败：" + serial->lastError(), kColorError);
}

void MainWindow::on_btnSend_clicked() {
  if (!serial->isOpen()) {
    appendNotice("请先打开串口", kColorError);
    return;
  }

  if (m_sendData.isEmpty())
    return; // 空内容不发，也不清空输入框

  const QByteArray payload = buildPayload();
  serial->write(payload);

  // 回显实际发送的字节：勾选「HEX 显示」时以十六进制展示，否则显示文本。
  appendRecord("TX →", kColorTx, formatForDisplay(payload));
}

void MainWindow::on_btnClear_clicked() {
  ui->recvEdit->clear();
  m_rxLineOpen = false;
  rxIdleTimer->stop();
}

// 「HEX 发送」只切换发送框的显示形式：勾选时把内容显示成十六进制（如 41 42），
// 但真正发送的字节（m_sendData）不变。
void MainWindow::on_checkHexSend_toggled(bool checked) {
  m_updatingSend = true;
  ui->editSend->setPlainText(checked ? bytesToHexText(m_sendData)
                                     : QString::fromUtf8(m_sendData));
  m_updatingSend = false;
}

// 发送框内容变化时同步逻辑内容 m_sendData：
// 十六进制显示模式下，框内文本按十六进制解码成字节；否则按 UTF-8 文本。
void MainWindow::on_editSend_textChanged() {
  if (m_updatingSend)
    return; // 由显示切换或自动格式化引起的改动，不再反向处理

  if (ui->checkHexSend->isChecked()) {
    // HEX 模式：把内容规范化成「大写、两位一组、空格分隔」，
    // 非十六进制字符自动丢弃（g、v、空格等都不作数）。
    const QString normalized = normalizeHexInput(ui->editSend->toPlainText());
    if (normalized != ui->editSend->toPlainText()) {
      m_updatingSend = true;
      ui->editSend->setPlainText(normalized);
      QTextCursor cursor = ui->editSend->textCursor();
      cursor.movePosition(QTextCursor::End); // 光标移到末尾
      ui->editSend->setTextCursor(cursor);
      m_updatingSend = false;
    }
    m_sendData = hexTextToBytes(normalized);
  } else {
    m_sendData = ui->editSend->toPlainText().toUtf8();
  }
}

// 收到数据：只有出现换行符才另起一行，否则接在当前行末尾（连续数据不换行）。
void MainWindow::onSerialData(const QByteArray &data) {
  const QString html = contentToHtml(formatForDisplay(data));
  const bool stick = isAtBottom();

  if (m_rxLineOpen) {
    // 上一包没有以换行结束：直接追加到本行末尾，不新增一行、不再打时间戳。
    // HEX 模式分片之间补一个空格，避免 "01 02" + "03" 拼成 "01 0203"。
    const QString sep =
        (ui->checkHexDisplay->isChecked() && !html.isEmpty()) ? " " : "";
    QTextCursor cursor(ui->recvEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(QString("<span style='color:%1;'>%2%3</span>")
                          .arg(kColorRx, sep, html));
  } else {
    // 新的一行：带时间戳与 "RX ←" 前缀。
    const QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
    ui->recvEdit->appendHtml(
        QString("<span style='color:%1;'>%2 RX ←<br>%3</span>")
            .arg(kColorRx, time, html));
  }

  // 以换行结尾 -> 立即关闭本行；否则保持打开，并启动空闲计时：
  // 若在设定毫秒数内没有新数据，计时器超时后自动换行。
  m_rxLineOpen = !endsWithLineBreak(data);
  const int idleMs = ui->spinRxTimeout->value();
  if (m_rxLineOpen && idleMs > 0)
    rxIdleTimer->start(idleMs); // 每来一段数据都重置计时
  else
    rxIdleTimer->stop();

  if (stick)
    scrollToBottom();
}

void MainWindow::onSerialError(const QString &message) {
  appendNotice(message, kColorError);
}

void MainWindow::onConnectionChanged(bool open) {
  ui->btnOpen->setText(open ? "关闭串口" : "打开串口");
  ui->comboPort->setEnabled(!open);
  ui->comboBaud->setEnabled(!open);
  ui->btnRefresh->setEnabled(!open);

  appendNotice(open ? "串口已打开：" + ui->comboPort->currentText() : "串口已关闭",
               kColorInfo);
}

// 按当前显示模式把字节转成可读文本。
QString MainWindow::formatForDisplay(const QByteArray &data) const {
  if (ui->checkHexDisplay->isChecked())
    return QString::fromLatin1(data.toHex(' ')).toUpper(); // 如 "48 65 6C"
  return QString::fromUtf8(data);                          // 按 UTF-8 文本
}

// 组装真正要发送的字节：发送框的逻辑内容（m_sendData）+ 所选行尾符。
QByteArray MainWindow::buildPayload() const {
  QByteArray payload = m_sendData;
  payload.append(ui->comboLineEnd->currentData().toByteArray()); // 追加行尾符
  return payload;
}

// 追加一条带时间戳的收发记录（内容另起一行显示）。
void MainWindow::appendRecord(const QString &tag, const QString &color,
                              const QString &content) {
  m_rxLineOpen = false; // 插入新的一行后，接收行不再连续
  rxIdleTimer->stop();
  const bool stick = isAtBottom(); // 追加前记录用户是否停在底部
  const QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
  ui->recvEdit->appendHtml(QString("<span style='color:%1;'>%2 %3<br>%4</span>")
                               .arg(color, time, tag, contentToHtml(content)));
  if (stick)
    scrollToBottom();
}

// 追加一条提示（如「已打开」「打开失败」）。
void MainWindow::appendNotice(const QString &msg, const QString &color) {
  m_rxLineOpen = false; // 插入新的一行后，接收行不再连续
  rxIdleTimer->stop();
  const bool stick = isAtBottom();
  const QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
  ui->recvEdit->appendHtml(QString("<span style='color:%1;'>%2 %3</span>")
                               .arg(color, time, msg.toHtmlEscaped()));
  if (stick)
    scrollToBottom();
}

// 判断接收区滚动条是否已经位于最底部。
bool MainWindow::isAtBottom() const {
  const QScrollBar *bar = ui->recvEdit->verticalScrollBar();
  return bar->value() >= bar->maximum();
}

void MainWindow::scrollToBottom() {
  QScrollBar *bar = ui->recvEdit->verticalScrollBar();
  bar->setValue(bar->maximum());
}
