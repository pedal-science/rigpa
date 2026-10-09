// RigSession against a real namchaind started with no rig: the startup rule loads the first rig
// in the list, and a rig loaded by another client is followed.
#include <QtTest>

#include "LiveDaemon.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

class TestRigLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    quint16 &port = daemon.port;
    QString &token = daemon.token;

private slots:
    // No rig at start: the engine runs nothing.
    void initTestCase() { daemon.start({"Clean", "Lead"}); }

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
