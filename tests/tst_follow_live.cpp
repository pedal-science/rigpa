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
#include "rig/NodeEditorModel.h"
#include "rig/PaletteModel.h"
#include "rig/PlayModel.h"
#include "rig/RigEdits.h"
#include "rig/Templates.h"
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
    Templates templates;
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
        templates.follow(&pedald, &events);
        editor.setTemplates(&templates);
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

    // A block from the pedal's templates: added, named by its template, and edited with its own
    // controls, which reach the engine as the engine's values.
    void aTemplateBlockIsEditedWithItsOwnControls()
    {
        QTRY_COMPARE_WITH_TIMEOUT(templates.all().size(), 27, 5000);
        PaletteModel palette(&engine, nullptr);
        palette.setTemplates(&templates);
        const auto made = palette.make(palette.indexOfType("eq.lowcut"));
        QVERIFY(made);
        RigDocument doc = session.document();
        QVERIFY(edits::insert(doc, *made, {0, {}, 0}));
        session.applyDocument(doc);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying() && session.findElement(made->element), 5000);
        QCOMPARE(editor.itemFor(made->element)->title, QStringLiteral("Low Cut"));

        NodeEditorModel node(&session, &engine, &editor, nullptr);
        node.setTemplates(&templates);
        node.open(made->element);
        QCOMPARE(node.rowCount(), 2);
        QCOMPARE(node.paramAt(0), QStringLiteral("freq"));
        QCOMPARE(node.valueAt(0), 80.0);
        node.setPosition(0, (200.0 - 20) / (500 - 20));
        node.release(0);
        const auto lowCut = [&] {
            for (const auto &n : session.document().json.value("chains").toArray()[0].toObject().value("nodes").toArray())
                if (n.toObject().value("element").toString() == made->element)
                    return n.toObject().value("low_cut").toDouble();
            return -1.0;
        };
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(lowCut() - 200) < 1, 5000);
        node.choose(1, 1); // 24 dB
        QTRY_VERIFY_WITH_TIMEOUT(!session.edits()->busy(), 5000);
        session.refresh();
        QTRY_COMPARE_WITH_TIMEOUT([&] {
            for (const auto &n : session.document().json.value("chains").toArray()[0].toObject().value("nodes").toArray())
                if (n.toObject().value("element").toString() == made->element)
                    return n.toObject().value("low_slope").toString();
            return QString();
        }(), QStringLiteral("24"), 5000);
        node.close();
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
