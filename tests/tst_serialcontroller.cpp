// ============================================================
//  tests/tst_serialcontroller.cpp —— SerialController 单元测试
//
//  跑法：ctest --test-dir build --output-on-failure
//
//  测什么？
//   1) availablePorts() 的契约：已排序、无重复；
//   2) 打开一个不存在的端口要失败，并给出错误描述；
//   3) 真收发：用 socat 造一对虚拟串口，验证「打开 → 写 → 对端收到 → 对端回 → 信号发出」。
//      没有 socat 的环境会自动跳过（不算失败）。
// ============================================================
#include "serialcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class TestSerialController : public QObject {
  Q_OBJECT

private slots:
  void availablePortsIsSortedAndUnique();
  void openInvalidPortFails();
  void virtualPortRoundTrip();
  void cleanup(); // 每个用例之后都会调用：收掉 socat、删掉临时链接

private:
  QProcess m_socat;
  QString m_left;  // 被测的一端
  QString m_right; // 对端

  bool startVirtualPorts();
};

void TestSerialController::availablePortsIsSortedAndUnique() {
  const QStringList ports = SerialController::availablePorts();

  QStringList sorted = ports;
  sorted.sort();
  QCOMPARE(ports, sorted); // 必须已排序

  QStringList unique = ports;
  unique.removeDuplicates();
  QCOMPARE(ports.size(), unique.size()); // 不能有重复项

  // 列出来的都应该是 ttyXXX 形式的名字，不该混进 tty、pts 之类
  for (const QString &name : ports) {
    QVERIFY2(!name.isEmpty(), "端口名不能为空");
    QVERIFY2(!name.contains('/'), qPrintable("端口名不该带路径: " + name));
  }
}

void TestSerialController::openInvalidPortFails() {
  SerialController controller;

  QVERIFY(!controller.open("/dev/definitely-not-a-port-9f3a", 115200));
  QVERIFY(!controller.isOpen());
  QVERIFY(!controller.lastError().isEmpty()); // 要有可读的失败原因
}

bool TestSerialController::startVirtualPorts() {
  if (QStandardPaths::findExecutable("socat").isEmpty())
    return false;

  const QString dir = QDir::temp().absolutePath();
  m_left = dir + "/tty-sa-test-0";
  m_right = dir + "/tty-sa-test-1";
  QFile::remove(m_left);
  QFile::remove(m_right);

  m_socat.start("socat",
                {"-d", "-d",
                 QStringLiteral("PTY,raw,echo=0,link=%1,perm=0666").arg(m_left),
                 QStringLiteral("PTY,raw,echo=0,link=%1,perm=0666").arg(m_right)});
  if (!m_socat.waitForStarted(5000))
    return false;

  // socat 建软链接需要一点时间：轮询等待，最多 5 秒
  //（这里不用 QTRY_VERIFY 之类的宏，它们在失败时会 return; ，不能用在有返回值的函数里）
  for (int i = 0; i < 100 && !(QFile::exists(m_left) && QFile::exists(m_right)); ++i)
    QTest::qWait(50);
  return QFile::exists(m_left) && QFile::exists(m_right);
}

void TestSerialController::virtualPortRoundTrip() {
  if (!startVirtualPorts())
    QSKIP("环境里没有 socat，跳过虚拟串口的收发测试");

  // 一端当成"上位机"（被测对象），另一端当成"设备"，两端都用 SerialController
  SerialController host;
  SerialController device;
  QSignalSpy hostSpy(&host, &SerialController::dataReceived);
  QSignalSpy deviceSpy(&device, &SerialController::dataReceived);
  QSignalSpy connSpy(&host, &SerialController::connectionChanged);

  QVERIFY2(host.open(m_left, 115200), qPrintable(host.lastError()));
  QVERIFY2(device.open(m_right, 115200), qPrintable(device.lastError()));

  // 打开成功要发一次 connectionChanged(true)
  QCOMPARE(connSpy.count(), 1);
  QCOMPARE(connSpy.at(0).at(0).toBool(), true);

  // 上位机 → 设备
  QVERIFY(host.write("hello\n") > 0);
  QTRY_COMPARE_WITH_TIMEOUT(deviceSpy.count(), 1, 2000);
  QCOMPARE(deviceSpy.at(0).at(0).toByteArray(), QByteArray("hello\n"));

  // 设备 → 上位机
  QVERIFY(device.write("world") > 0);
  QTRY_COMPARE_WITH_TIMEOUT(hostSpy.count(), 1, 2000);
  QCOMPARE(hostSpy.at(0).at(0).toByteArray(), QByteArray("world"));

  // 关闭后要发一次 connectionChanged(false)
  host.close();
  QVERIFY(!host.isOpen());
  QCOMPARE(connSpy.count(), 2);
  QCOMPARE(connSpy.at(1).at(0).toBool(), false);
}

void TestSerialController::cleanup() {
  if (m_socat.state() != QProcess::NotRunning) {
    m_socat.kill();
    m_socat.waitForFinished(2000);
  }
  if (!m_left.isEmpty()) {
    QFile::remove(m_left);
    QFile::remove(m_right);
  }
  m_left.clear();
  m_right.clear();
}

QTEST_MAIN(TestSerialController)
#include "tst_serialcontroller.moc"
