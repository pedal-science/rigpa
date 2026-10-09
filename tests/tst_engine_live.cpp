// The client against the real namchaind (null backend), started on a kernel-chosen port with a
// fresh token, as namchain's own tests/cli/run_ctl.cmake does. Skipped when RIGPA_NAMCHAIND
// doesn't name a built daemon.
#include <QJsonArray>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "engine/EngineSession.h"

using rigpa::EngineClient;
using rigpa::EngineReply;
using rigpa::EngineSession;

class TestEngineLive : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    QProcess daemon;
    quint16 port = 0;
    QString token = QStringLiteral("rigpa-live-test-token-0123456789");

private slots:
    void initTestCase()
    {
        const QString namchaind = qEnvironmentVariable("RIGPA_NAMCHAIND");
        const QString models = qEnvironmentVariable("RIGPA_NAMCHAIN_MODELS");
        if (namchaind.isEmpty() || !QFile::exists(namchaind) || !QFile::exists(models + "/A2-Lite.nam"))
            QSKIP("no built namchaind (RIGPA_NAMCHAIND) or test models (RIGPA_NAMCHAIN_MODELS)");

        QDir().mkpath(dir.filePath("rigs"));
        QFile::copy(QFINDTESTDATA("../dev/rigs/Clean.json"), dir.filePath("rigs/Clean.json"));
        QDir().mkpath(dir.filePath("lib/models"));
        QDir().mkpath(dir.filePath("lib/irs"));
        QFile::copy(models + "/A2-Lite.nam", dir.filePath("lib/models/A2-Lite.nam"));
        QFile::copy(models + "/CabIR.nam", dir.filePath("lib/irs/CabIR.nam"));
        QFile tokenFile(dir.filePath("token"));
        QVERIFY(tokenFile.open(QIODevice::WriteOnly));
        tokenFile.write(token.toUtf8() + "\n");
        tokenFile.close();
        tokenFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        // A free port: listen, note it, close. The daemon takes it a moment later.
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            port = probe.serverPort();
        }
        daemon.setWorkingDirectory(dir.path()); // the socket path is relative (macOS's 104 bytes)
        daemon.setProcessChannelMode(QProcess::MergedChannels);
        daemon.start(namchaind, {"--backend", "null", "--no-lock-memory", "--rt-priority", "0",
                                 "--capture-channels", "2", "--playback-channels", "2",
                                 "--socket", "d.sock", "--rigs-root", dir.filePath("rigs"),
                                 "--rig", "Clean.json", "--models-root", dir.filePath("lib/models"),
                                 "--irs-root", dir.filePath("lib/irs"), "--tcp-listen",
                                 "--tcp-address", "127.0.0.1", "--tcp-port", QString::number(port),
                                 "--tcp-token-file", dir.filePath("token")});
        QVERIFY(daemon.waitForStarted());
    }

    void cleanupTestCase()
    {
        if (daemon.state() != QProcess::NotRunning) {
            daemon.terminate();
            if (!daemon.waitForFinished(3000))
                daemon.kill();
        }
    }

    void sessionSeesTheRunningRig()
    {
        EngineSession session;
        session.client()->setBackoff({200});
        session.start("127.0.0.1", port, token);
        QTRY_VERIFY_WITH_TIMEOUT(session.connection() == EngineSession::Connected, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(session.rigName() == QLatin1String("Clean"), 5000);
        QCOMPARE(session.engineState(), QStringLiteral("running"));
        QTRY_VERIFY_WITH_TIMEOUT(session.dspPercent() >= 0, 5000);
        QVERIFY(session.client()->hasFeature("levels"));
    }

    void wrongTokenIsRefused()
    {
        EngineClient client;
        client.connectTo("127.0.0.1", port, "not-the-token-not-the-token");
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), EngineClient::State::Refused, 10000);
    }
};

QTEST_GUILESS_MAIN(TestEngineLive)
#include "tst_engine_live.moc"
