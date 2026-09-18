#include "mainwindow.h"

#include <QApplication>

// 全局样式表（QSS，语法类似 CSS）：统一美化默认控件，
// 让界面呈现扁平、现代的观感。深色区域专门用于接收区，便于阅读日志。
static const char *kAppStyleSheet = R"(
QMainWindow                   { background-color: #f5f6f8; }
QWidget#centralwidget         { background-color: #f5f6f8; }
QLabel                        { color: #333333; }

QComboBox, QLineEdit          { background: #ffffff; color: #333333;
                                border: 1px solid #d0d4da;
                                border-radius: 6px; padding: 4px 8px; min-height: 22px; }
QComboBox:hover, QLineEdit:hover { border-color: #2d8cf0; }

/* 下拉弹出列表：只设置「视图级」配色即可（普通/选中两项都要给全）。
   切记不要再写 ::item 子控件规则——那会接管列表项绘制，
   反而把选中/悬浮项的配色盖掉，出现「悬浮项没有文字」的怪现象。 */
QComboBox QAbstractItemView   { background: #ffffff; color: #333333; outline: 0;
                                border: 1px solid #d0d4da;
                                selection-background-color: #2d8cf0;
                                selection-color: #ffffff; }

QPlainTextEdit                { background: #1e2127; color: #e6e6e6;
                                border: 1px solid #d0d4da; border-radius: 6px;
                                font-family: "Consolas", "DejaVu Sans Mono", monospace;
                                font-size: 12px; padding: 6px; }

QPushButton                   { background: #2d8cf0; color: #ffffff; border: none;
                                border-radius: 6px; padding: 6px 16px; min-height: 22px; }
QPushButton:hover             { background: #4ba0f5; }
QPushButton:pressed           { background: #1c72d0; }
QPushButton:disabled          { background: #c2c8d0; }

QCheckBox                     { color: #333333; spacing: 6px; }
)";

int main(int argc, char *argv[]) {
  QApplication app(argc, argv); // 每个 Qt GUI 程序有且只有一个 QApplication
  app.setStyleSheet(kAppStyleSheet);

  MainWindow window; // 创建主窗口
  window.show();     // 显示窗口

  return QApplication::exec(); // 进入事件循环，直到窗口关闭
}
