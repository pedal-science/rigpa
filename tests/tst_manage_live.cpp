// Rig management and auto-save against a real namchaind and pedald: an edit is saved to the
// library about a second later; New, Rename (of the running rig), Copy and Delete go through
// pedald; a rig that isn't in the library is saved with Save as.
#include <QJsonArray>
#include <QtTest>

#include "LiveDaemon.h"
#include "pedald/PedaldClient.h"
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
        rigs.start();
        engine.start("127.0.0.1", daemon.port, daemon.token);
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
        manager.begin("duplicate");
        QCOMPARE(manager.proposedName(), QStringLiteral("Sparkle copy"));
        manager.confirm();
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(rigs.names().contains("Sparkle copy"), 5000);
        QCOMPARE(stored("Sparkle copy").value("name").toString(), QStringLiteral("Sparkle copy"));
        QCOMPARE(rigs.running(), QStringLiteral("Sparkle")); // a copy isn't loaded

        session.loadRigNamed("Sparkle copy");
        QTRY_COMPARE_WITH_TIMEOUT(engine.documentName(), QStringLiteral("Sparkle copy"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QStringLiteral("Sparkle copy"), 5000);
        manager.begin("delete");
        manager.ring()->activated(0); // apply on OK
        QVERIFY(settled());
        QTRY_VERIFY_WITH_TIMEOUT(!rigs.names().contains("Sparkle copy"), 5000);
        // Still playing, but no longer in the library (once the engine's next status is in).
        QTRY_COMPARE_WITH_TIMEOUT(rigs.running(), QString(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.saveState(), QStringLiteral("unsaved"), 5000);
    }

    void saveAsKeepsAnUnsavedRig()
    {
        // An edit to a rig the library doesn't have is not saved anywhere.
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
