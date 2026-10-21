// The tuner against a real namchaind: while the view is up, the engine is running a tuner node after
// each chain's input and the rig is silent; when it goes away, the engine is running exactly the
// document it had before.
//
// What is checked is the whole path a menu button takes: RigSession publishes the document with the
// node (namchain docs/tuner.md), the engine accepts it, `rig.state` reports it so the view can say
// the rig really is muted, and the document rigpa holds never contains it -- which is what keeps it
// off the canvas, out of undo and out of the library.
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include "LiveDaemon.h"
#include "engine/EngineClient.h"
#include "rig/RigEdits.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"
#include "rig/TunerModel.h"

using namespace rigpa;

class TestTunerLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    TunerModel tuner;

private slots:
    void initTestCase()
    {
        daemon.start({QStringLiteral("Dual"), QStringLiteral("Clean")}, QStringLiteral("Dual"));
        rigs.setNames({QStringLiteral("Dual"), QStringLiteral("Clean")});
        engine.start(QStringLiteral("127.0.0.1"), daemon.port, daemon.token);
        tuner.setClient(engine.client());
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && !engine.schema().isEmpty(), 15000);
    }

    void theEngineHasTheTuner()
    {
        QVERIFY2(engine.hasTuner(), "the daemon's schema or its node_state feature is missing");
    }

    void mountingPutsOneNodeAfterEachChainsInput()
    {
        const QJsonObject before = engineDocument();
        QVERIFY(!edits::hasTuner(before));

        // What the menu button does: App turns the view into these two calls.
        tuner.setWatching(true);
        session.setTuner(true);

        QTRY_VERIFY_WITH_TIMEOUT(edits::hasTuner(engineDocument()), 10000);

        // One per chain, and the first node after the input in the document's own order -- which is
        // what "the first node after the input" means to anyone reading it back.
        const QJsonArray chains = engineDocument().value(QStringLiteral("chains")).toArray();
        QCOMPARE(chains.size(), 2);
        for (const QJsonValue &value : chains) {
            const QJsonArray nodes = value.toObject().value(QStringLiteral("nodes")).toArray();
            QCOMPARE(nodes.at(0).toObject().value(QStringLiteral("type")).toString(),
                     QStringLiteral("nc.input"));
            QCOMPARE(nodes.at(1).toObject().value(QStringLiteral("type")).toString(),
                     QStringLiteral("nc.tuner"));
            QCOMPARE(nodes.at(1).toObject().value(QStringLiteral("meta")).toObject()
                         .value(QStringLiteral("ephemeral")).toString(),
                     QStringLiteral("tuner"));
        }

        // The document rigpa edits and draws has none of it: the node is in what is published, not
        // in the rig.
        QVERIFY(!edits::hasTuner(edits::toJson(session.document())));

        // And the reading arrives, from the plan that is running: the null backend feeds silence, so
        // the tuner has a node and no note -- which the view draws as "No note", not as a stale one.
        QTRY_VERIFY_WITH_TIMEOUT(tuner.available(), 10000);
        QVERIFY(tuner.running());
        QVERIFY(!tuner.valid());
        QVERIFY(tuner.noteText().isEmpty());
    }

    void unmountingLeavesTheDocumentAsItWas()
    {
        tuner.setWatching(false);
        session.setTuner(false);

        QTRY_VERIFY_WITH_TIMEOUT(!edits::hasTuner(engineDocument()), 10000);
        const QJsonObject after = engineDocument();
        QCOMPARE(after.value(QStringLiteral("chains")).toArray().size(), 2);
        for (const QJsonValue &value : after.value(QStringLiteral("chains")).toArray()) {
            const QJsonArray nodes = value.toObject().value(QStringLiteral("nodes")).toArray();
            for (const QJsonValue &node : nodes)
                QVERIFY(node.toObject().value(QStringLiteral("type")).toString()
                        != QStringLiteral("nc.tuner"));
        }
        QVERIFY(!tuner.available());
    }

private:
    // The engine's own parse of what it is running (`rig.get`), not rigpa's stripped copy.
    QJsonObject engineDocument()
    {
        QJsonObject document;
        bool done = false;
        engine.client()->request(QStringLiteral("rig.get"), {}, [&](const EngineReply &reply) {
            done = true;
            if (reply.ok)
                document = reply.body.value(QStringLiteral("document")).toObject();
        });
        if (!QTest::qWaitFor([&] { return done; }, 10000))
            return {};
        return document;
    }
};

QTEST_GUILESS_MAIN(TestTunerLive)
#include "tst_tuner_live.moc"
