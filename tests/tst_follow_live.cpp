// rigpa following the pedal, against a real namchaind and pedald: another app's edit reaches the
// screen as the engine pushes it, a press shows on the play line with its cause, and undo is the
// pedal's, so it undoes another app's edit too.
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include "LiveDaemon.h"
#include "engine/EngineSession.h"
#include "pedald/PedaldClient.h"
#include "pedald/PedaldEvents.h"
#include "rig/PlayModel.h"
#include "rig/RigEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

namespace {
const QString kCab = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000002");
}

class TestFollowLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    PedaldEvents events{&pedald};
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    RigEditorModel editor{&session, nullptr};
    PlayModel play{&session, &editor, &engine};
    EngineClient ljoss;

    QJsonObject cabNode()
    {
        for (const auto &c : session.document().json.value("chains").toArray())
            for (const auto &n : c.toObject().value("nodes").toArray())
                if (n.toObject().value("element").toString() == kCab)
                    return n.toObject();
        return {};
    }

    bool ljossSets(const QString &param, double value)
    {
        const QJsonObject cab = cabNode();
        bool done = false, ok = false;
        ljoss.request("rig.patch", {{"chain", session.document().chains[0].id},
                                    {"ops", QJsonArray{QJsonObject{{"op", "set_param"}, {"node", cab.value("id")},
                                                                   {"param", param}, {"value", value}}}}},
                      [&](const EngineReply &r) { ok = r.ok; done = true; });
        return QTest::qWaitFor([&] { return done; }, 5000) && ok;
    }

private slots:
    void initTestCase()
    {
        daemon.start({"Clean", "Lead"}, "Clean");
        daemon.startPedald();
        pedald.setEndpoint("127.0.0.1", daemon.pedaldPort, daemon.token);
        rigs.setEvents(&events);
        session.setPedal(&pedald, &events);
        play.setEvents(&events);
        rigs.start();
        events.start();
        engine.start("127.0.0.1", daemon.port, daemon.token);
        ljoss.setClientName("ljoss");
        ljoss.connectTo("127.0.0.1", daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(events.connected() && engine.following() && ljoss.isReady(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && rigs.running() == "Clean", 15000);
    }

    void anotherAppsEditArrivesAtOnce()
    {
        const double before = cabNode().value("level_db").toDouble();
        const double wanted = before == -3.0 ? -4.0 : -3.0;
        QElapsedTimer clock;
        clock.start();
        QVERIFY(ljossSets("level_db", wanted));
        QTRY_COMPARE_WITH_TIMEOUT(cabNode().value("level_db").toDouble(), wanted, 1000);
        // The plan's bar: under 100 ms from the request going out to the screen's document.
        QVERIFY2(clock.elapsed() < 100, qPrintable(QString::number(clock.elapsed()) + " ms"));
        QTRY_VERIFY2_WITH_TIMEOUT(play.lastChange().startsWith("Ljoss: "), qPrintable(play.lastChange()), 2000);
    }

    void undoIsThePedalsAndTakesAnotherAppsEdit()
    {
        QVERIFY(session.pedalUndo());
        QTRY_VERIFY_WITH_TIMEOUT(session.canUndo(), 5000);
        const double edited = cabNode().value("level_db").toDouble();
        session.undo();
        QTRY_VERIFY_WITH_TIMEOUT(cabNode().value("level_db").toDouble() != edited, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(session.canRedo(), 5000);
        session.redo();
        QTRY_COMPARE_WITH_TIMEOUT(cabNode().value("level_db").toDouble(), edited, 5000);
    }

    void aPressShowsWithItsCause()
    {
        bool done = false;
        pedald.post("/event", QJsonDocument(QJsonObject{{"kind", 1}}).toJson(), [&](const PedaldReply &) { done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(play.lastChange(), QStringLiteral("Rig up → Lead"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Lead"), 5000);
    }

    void loadingARigGoesThroughThePedal()
    {
        session.loadRigNamed("Clean");
        QTRY_COMPARE_WITH_TIMEOUT(engine.documentName(), QStringLiteral("Clean"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(play.lastChange(), QStringLiteral("Rigpa → Clean"), 5000);
    }
};

QTEST_MAIN(TestFollowLive)
#include "tst_follow_live.moc"
