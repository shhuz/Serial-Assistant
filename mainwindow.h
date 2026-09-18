#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  void on_btnOpen_clicked();
  void on_btnSend_clicked();
  void on_btnClear_clicked();
  void readData();

private:
  Ui::MainWindow *ui;
  QSerialPort serial;

  void log(const QString &msg, const QString &color = "#95a5a6");
  void scrollToBottom();
};

#endif // MAINWINDOW_H
