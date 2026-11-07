// The compact layout's play view against a real namchaind and pedald: the chips are the running
// rig's blocks, the pedal service says where the footswitches are, and a press (pedald's POST
// /event, the same event a footswitch sends) moves the rig, the bank position and the play view
// with it.
#include <QJsonDocument>
#include <QtTest>

#include "LiveDaemon.h"
#include "engine/EngineSession.h"
#include "pedald/PedaldClient.h"
#include "pedald/PedaldStatus.h"
#include "rig/PlayModel.h"
#include "rig/RigEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

class TestPlayLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    PedaldStatus status{&pedald};
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    RigEditorModel editor{&session, nullptr};
    PlayModel play{&session, &editor, &engine};

    // A footswitch press, as the Pico would send it: `cc` 81 at 127 is "rig up" in the default map.
    bool press(int controller)
    {
        bool done = false, ok = false;
        const QByteArray body = QJsonDocument(QJsonObject{{"kind", 0x10}, {"a", controller}, {"b", 127}}).toJson();
        pedald.post(QStringLiteral("/event"), body, [&](const PedaldReply &r) {
            ok = r.ok;
            done = true;
        });
        return QTest::qWaitFor([&] { return done; }, 5000) && ok;
    }

private slots:
    void initTestCase()
    {
        daemon.start({"Clean", "Flow", "Lead"}, "Clean");
        daemon.startPedald();
        pedald.setEndpoint("127.0.0.1", daemon.pedaldPort, daemon.token);
        rigs.start();
        status.start();
        engine.start("127.0.0.1", daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && engine.rigName() == "Clean", 15000);
    }

    void theChipsAreTheRunningRigsBlocks()
    {
        QTRY_COMPARE_WITH_TIMEOUT(play.count(), 2, 5000); // Clean: the amp and the cab
        QVERIFY(!play.data(play.index(0), PlayModel::TitleRole).toString().isEmpty());
    }

    void theServiceSaysWhereTheFootswitchesAre()
    {
        // With no bank set, the bank is the library by name: Clean, Flow, Lead.
        QTRY_VERIFY_WITH_TIMEOUT(status.reachable() && status.size() == 3, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(status.position(), 0, 10000);
        QCOMPARE(status.bank(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(status.next(), QStringLiteral("Flow"), 10000);
    }

    void aPressMovesTheRigAndThePlayViewFollows()
    {
        QVERIFY(press(81));
        QTRY_COMPARE_WITH_TIMEOUT(engine.rigName(), QStringLiteral("Flow"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(status.position(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(play.lastChange(), QStringLiteral("Now playing Flow"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(status.next(), QStringLiteral("Lead"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(play.count() > 0, 5000);
    }
};

QTEST_MAIN(TestPlayLive)
#include "tst_play_live.moc"
