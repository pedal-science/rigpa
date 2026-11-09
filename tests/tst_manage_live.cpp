// Rig management against a real namchaind and pedald, which saves what is played: an edit shows as
// pending until pedald says it saved it, about a second later; New, Rename (of the running rig),
// Copy and Delete go through pedald, and deleting the playing rig moves the pedal on.
#include <QJsonArray>
#include <QtTest>

#include "LiveDaemon.h"
#include "pedald/PedaldClient.h"
#include "pedald/PedaldEvents.h"
#include "rig/RigEdits.h"
#include "rig/RigList.h"
#include "rig/RigManager.h"
#include "rig/RigSession.h"

using namespace rigpa;

namespace {
const QString kCab = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000002");
}

class TestManageLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    PedaldEvents events{&pedald};
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    RigManager manager{&engine, &session, &rigs, &pedald};

    // The library's copy of a rig, as pedald has it.
    QJsonObject stored(const QString &name)
    {
        QJsonObject out;
        bool done = false;
        pedald.get(PedaldClient::rigPath(name), [&](const PedaldReply &r) {
            out = r.ok ? r.body.object() : QJsonObject{};
            done = true;
        });
        if (!QTest::qWaitFor([&] { return done; }, 5000))
            return {};
        return out;
    }

    bool settled()
    {
        return QTest::qWaitFor([&] { return !manager.busy() && manager.dialog().isEmpty(); }, 8000);
    }

private slots:
    void initTestCase()
    {
        daemon.start({"Clean", "Lead"}, "Clean");
        daemon.startPedald();
        pedald.setEndpoint("127.0.0.1", daemon.pedaldPort, daemon.token);
        rigs.setEvents(&events);
        session.setPedal(&pedald, &events);
        manager.setEvents(&events);
        rigs.start();
        events.start();
        engine.start("127.0.0.1", daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(events.connected(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(rigs.loaded() && rigs.count() == 2, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && rigs.running() == "Clean", 15000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("saved"), 5000);
    }

    void anEditIsSavedASecondLater()
    {
        session.setBypass(kCab, true);
        QCOMPARE(manager.saveState(), QStringLiteral("pending"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("saved"), 5000);
        bool bypassed = false;
        for (const auto &n : stored("Clean").value("chains").toArray()[0].toObject().value("nodes").toArray())
            if (n.toObject().value("id").toString() == kCab + "-cab")
                bypassed = n.toObject().value("bypass").toBool();
        QVERIFY(bypassed);
        QCOMPARE(stored("Clean").value("name").toString(), QStringLiteral("Clean"));
    }

    void namesAreChecked()
    {
        QVERIFY(!manager.check("").isEmpty());
        QVERIFY(!manager.check("last").isEmpty());
        QVERIFY(!manager.check(".hidden").isEmpty());
        QVERIFY(!manager.check("a/b").isEmpty());
        QVERIFY(!manager.check("lead").isEmpty()); // a clash, whatever the case
        QVERIFY(manager.check("Crunch").isEmpty());
    }

    void renameTheRunningRig()
    {
        const qint64 generation = engine.generation();
        manager.begin("rename");
        QCOMPARE(manager.dialog(), QStringLiteral("rename"));
        QCOMPARE(manager.proposedName(), QStringLiteral("Clean"));
        manager.confirm("Lead");
        QVERIFY(!manager.dialogError().isEmpty()); // taken: the dialog stays, saying so
        manager.confirm("Sparkle");
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(rigs.names().contains("Sparkle") && !rigs.names().contains("Clean"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Sparkle"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(session.document().name, QStringLiteral("Sparkle"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        // A name is not a change of sound: the engine rebuilt nothing.
        QCOMPARE(engine.generation(), generation);
        QCOMPARE(stored("Sparkle").value("name").toString(), QStringLiteral("Sparkle"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("saved"), 5000);
    }

    void copyThenDelete()
    {
        // The copy is named from the running rig, so that has to have settled first: on a loaded
        // build machine a status from before the rename can still be on its way (an image build
        // failed here once, 2026-10-10).
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Sparkle"), 5000);
        manager.begin("duplicate");
        QCOMPARE(manager.proposedName(), QStringLiteral("Sparkle copy"));
        manager.confirm();
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(rigs.names().contains("Sparkle copy"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(stored("Sparkle copy").value("name").toString(), QStringLiteral("Sparkle copy"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Sparkle"), 5000); // a copy isn't loaded

        session.loadRigNamed("Sparkle copy");
        QTRY_COMPARE_WITH_TIMEOUT(engine.documentName(), QStringLiteral("Sparkle copy"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Sparkle copy"), 5000);
        manager.begin("delete");
        manager.ring()->activated(0); // apply on OK
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(!rigs.names().contains("Sparkle copy"), 5000);
        // The pedal moves to the bank's next rig, as a press would.
        QTRY_VERIFY_WITH_TIMEOUT(!rigs.running().isEmpty() && rigs.running() != "Sparkle copy", 5000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("saved"), 5000);
    }

    void saveAsKeepsAnUnsavedRig()
    {
        // A rig the library doesn't have: what an older app applied.
        QJsonObject document = session.document().json;
        document.insert("name", "Sketch");
        bool applied = false;
        engine.client()->request("rig.apply", {{"document", document}}, [&](const EngineReply &r) { applied = r.ok; });
        QTRY_VERIFY_WITH_TIMEOUT(applied, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(session.document().name, QStringLiteral("Sketch"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("unsaved"), 5000);
        session.setBypass(kCab, false);
        QCOMPARE(manager.saveState(), QStringLiteral("unsaved"));
        manager.begin("saveAs");
        QCOMPARE(manager.dialog(), QStringLiteral("saveAs"));
        manager.confirm("Kept");
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(rigs.names().contains("Kept"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Kept"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("saved"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(session.document().name, QStringLiteral("Kept"), 5000);
    }

    void newRigIsLoaded()
    {
        manager.begin("new");
        QCOMPARE(manager.proposedName(), QStringLiteral("New rig"));
        manager.confirm();
        QVERIFY(settled());
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("New rig"), 8000);
        QTRY_COMPARE_WITH_TIMEOUT(session.document().name, QStringLiteral("New rig"), 5000);
        QCOMPARE(session.document().chains.size(), 1);
        QVERIFY(session.document().chains[0].editable);
        QVERIFY(session.document().chains[0].series.isEmpty());
    }

    void cancelLeavesEverything()
    {
        QTRY_VERIFY_WITH_TIMEOUT(rigs.names().contains("New rig"), 5000);
        const QStringList before = rigs.names();
        manager.begin("delete");
        manager.ring()->activated(1); // Cancel
        QVERIFY(manager.dialog().isEmpty());
        QTest::qWait(300);
        QCOMPARE(rigs.names(), before);
    }
};

QTEST_GUILESS_MAIN(TestManageLive)
#include "tst_manage_live.moc"
