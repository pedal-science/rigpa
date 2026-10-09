// RigSession against a real namchaind started with no rig: the startup rule loads the first rig
// in the list, and a rig loaded by another client is followed.
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

class TestRigLive : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QProcess daemon;
    quint16 port = 0;
    QString token = QStringLiteral("rigpa-rig-live-token-0123456789");

private slots:
    void initTestCase()
    {
        const QString namchaind = qEnvironmentVariable("RIGPA_NAMCHAIND");
        const QString models = qEnvironmentVariable("RIGPA_NAMCHAIN_MODELS");
        if (namchaind.isEmpty() || !QFile::exists(namchaind) || !QFile::exists(models + "/A2-Lite.nam"))
            QSKIP("no built namchaind (RIGPA_NAMCHAIND) or test models (RIGPA_NAMCHAIN_MODELS)");
        QDir().mkpath(dir.filePath("rigs"));
        for (const char *rig : {"Clean", "Lead"})
            QFile::copy(QFINDTESTDATA(QStringLiteral("../dev/rigs/%1.json").arg(QLatin1String(rig))),
                        dir.filePath(QStringLiteral("rigs/%1.json").arg(QLatin1String(rig))));
        QDir().mkpath(dir.filePath("lib"));
        QFile::copy(models + "/A2-Lite.nam", dir.filePath("lib/A2-Lite.nam"));
        QFile t(dir.filePath("token"));
        QVERIFY(t.open(QIODevice::WriteOnly));
        t.write(token.toUtf8());
        t.close();
        t.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            port = probe.serverPort();
        }
        daemon.setWorkingDirectory(dir.path());
        daemon.setProcessChannelMode(QProcess::MergedChannels);
        // No --rig: the engine starts running nothing.
        daemon.start(namchaind, {"--backend", "null", "--no-lock-memory", "--rt-priority", "0",
                                 "--socket", "d.sock", "--rigs-root", dir.filePath("rigs"),
                                 "--models-root", dir.filePath("lib"), "--irs-root", dir.filePath("lib"),
                                 "--tcp-listen", "--tcp-address", "127.0.0.1", "--tcp-port",
                                 QString::number(port), "--tcp-token-file", dir.filePath("token")});
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

    void startupLoadsTheFirstRigThenFollowsOthers()
    {
        EngineSession engine;
        PedaldClient pedald; // unused: the list is set by hand
        RigList rigs(&pedald);
        RigSession session(&engine, &rigs);
        rigs.setNames({"Clean", "Lead"});
        engine.client()->setBackoff({200});
        engine.start("127.0.0.1", port, token);

        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument(), 15000);
        QCOMPARE(session.document().name, QStringLiteral("Clean"));
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Clean"), 5000);

        // Someone else (a footswitch through pedald, ljoss) loads another rig.
        EngineClient other;
        bool loaded = false;
        connect(&other, &EngineClient::ready, &other, [&] {
            other.request("rig.load", {{"rig", "Lead.json"}}, [&](const EngineReply &r) { loaded = r.ok; });
        });
        other.connectTo("127.0.0.1", port, token);
        QTRY_VERIFY_WITH_TIMEOUT(loaded, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(session.document().name, QStringLiteral("Lead"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Lead"), 5000);
    }

    // A rigpa that starts while a rig runs shows it, and loads nothing over it (the rule is only
    // for an engine running nothing). Runs after the test above, so the engine runs Lead.
    void startupLeavesARunningRigAlone()
    {
        EngineSession engine;
        PedaldClient pedald;
        RigList rigs(&pedald);
        RigSession session(&engine, &rigs);
        rigs.setNames({"Clean", "Lead"});
        engine.start("127.0.0.1", port, token);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument(), 15000);
        QCOMPARE(session.document().name, QStringLiteral("Lead"));
        const qint64 generation = engine.generation();
        QTest::qWait(1500);
        QCOMPARE(engine.generation(), generation);
        QCOMPARE(session.document().name, QStringLiteral("Lead"));
    }
};

QTEST_GUILESS_MAIN(TestRigLive)
#include "tst_rig_live.moc"
