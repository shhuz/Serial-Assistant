#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QDir>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  QDir devDir("/dev");
  QStringList filters;
  filters << "ttyS*" << "ttyUSB*" << "ttyACM*";
  QStringList found =
      devDir.entryList(filters, QDir::System | QDir::Files | QDir::Readable);
  for (const QString &p : found) {
    ui->comboPort->addItem(p);
  }
  ui->comboPort->setEditable(true);

  ui->comboBaud->addItems({"9600", "19200", "38400", "57600", "115200"});
  ui->comboBaud->setCurrentText("115200");

  connect(&serial, &QSerialPort::readyRead, this, &MainWindow::readData);
}

MainWindow::~MainWindow() {
  if (serial.isOpen())
    serial.close();
  delete ui;
}

void MainWindow::on_btnOpen_clicked() {
  if (serial.isOpen()) {
    serial.close();
    ui->btnOpen->setText("打开串口");
    ui->comboPort->setEnabled(true);
    ui->comboBaud->setEnabled(true);
    log("[已关闭]", "#95a5a6");
  } else {
    QString port = ui->comboPort->currentText();
    if (!port.isEmpty() && !port.startsWith('/'))
      port = "/dev/" + port;
    serial.setPortName(port);
    serial.setBaudRate(ui->comboBaud->currentText().toInt());
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (serial.open(QIODevice::ReadWrite)) {
      ui->btnOpen->setText("关闭串口");
      ui->comboPort->setEnabled(false);
      ui->comboBaud->setEnabled(false);
      log("[已打开] " + ui->comboPort->currentText(), "#95a5a6");
    } else {
      log("[打开失败] " + serial.errorString(), "#e74c3c");
    }
  }
}

void MainWindow::readData() {
  QByteArray data = serial.readAll();
  QString content = ui->checkHex->isChecked()
                        ? QString::fromLatin1(data.toHex(' ')).toUpper()
                        : QString::fromUtf8(data);
  QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
  ui->recvEdit->appendHtml(
      QString("<span style='color:#2ecc71;'>%1 RX ←<br>%2</span>")
          .arg(time, content.toHtmlEscaped()));
  scrollToBottom();
}

void MainWindow::on_btnSend_clicked() {
  if (!serial.isOpen()) {
    log("[请先打开串口]", "#e74c3c");
    return;
  }
  QString text = ui->editSend->text();
  if (text.isEmpty())
    return;
  serial.write(text.toUtf8());
  QString shown = ui->checkHex->isChecked()
                      ? QString::fromLatin1(text.toUtf8().toHex(' ')).toUpper()
                      : text;
  QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
  ui->recvEdit->appendHtml(
      QString("<span style='color:#3498db;'>%1 TX →<br>%2</span>")
          .arg(time, shown.toHtmlEscaped()));
  scrollToBottom();
}

void MainWindow::on_btnClear_clicked() { ui->recvEdit->clear(); }

void MainWindow::log(const QString &msg, const QString &color) {
  QString time = QDateTime::currentDateTime().toString("[hh:mm:ss]");
  ui->recvEdit->appendHtml(QString("<span style='color:%1;'>%2 %3</span>")
                               .arg(color, time, msg.toHtmlEscaped()));
  scrollToBottom();
}

void MainWindow::scrollToBottom() {
  ui->recvEdit->verticalScrollBar()->setValue(
      ui->recvEdit->verticalScrollBar()->maximum());
}
