// flowEQ's own node editor against a real namchaind: opening a block shows the lite plugin's eight
// controls rather than the generic list, and the response curve is the *engine's* answer
// (`rig.state`) rather than anything this side computed.
//
// What is checked is the whole path a knob drag takes: a control moves, `rig.patch` ramps it, the
// node decodes the new latent, and the curve the panel binds to changes — plus the two things a
// panel must not do: keep claiming to know a node that is gone, and repaint when nothing moved.
#include <QJsonArray>
#include <QtTest>

#include "LiveDaemon.h"
#include "rig/FlowEqModel.h"
#include "rig/NodeEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

namespace {

// The Flow fixture's one selectable block: its element is its node id.
const QString kFlow = QStringLiteral("flow");

} // namespace

class TestFlowEqLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    NodeEditorModel editor{&session, &engine, nullptr, nullptr};

    FlowEqModel *flow() { return editor.flowEq(); }

private slots:
    void initTestCase()
    {
        daemon.start({QStringLiteral("Flow"), QStringLiteral("Clean")}, QStringLiteral("Flow"));
        rigs.setNames({QStringLiteral("Flow"), QStringLiteral("Clean")});
        engine.start(QStringLiteral("127.0.0.1"), daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && !engine.schema().isEmpty(), 15000);
    }

    void theBlockGetsItsOwnControls()
    {
        editor.open(kFlow);
        QVERIFY(editor.isOpen());
        QVERIFY(editor.isFlowEq());
        // The lite plugin's controls, in the order FlowEqPanel.qml lays them out -- which is also
        // the order of the ring, so the hardware rotary visits them in reading order.
        const QStringList expected{QStringLiteral("x"),      QStringLiteral("y"),
                                   QStringLiteral("z"),      QStringLiteral("strength"),
                                   QStringLiteral("in_db"),  QStringLiteral("out_db"),
                                   QStringLiteral("extend"), QStringLiteral("auto_gain")};
        QCOMPARE(editor.rowCount(), int(expected.size()));
        for (int i = 0; i < expected.size(); ++i) {
            QCOMPARE(editor.paramAt(i), expected.at(i));
            QCOMPARE(editor.indexOfParam(expected.at(i)), i);
        }
        QCOMPARE(editor.indexOfParam(QStringLiteral("nope")), -1);
        QCOMPARE(editor.ring()->count(), editor.rowCount() + 3); // Bypass, Close, Remove

        // The values are the document's, read through the same schema-driven reader the generic
        // editor uses: strength is a fraction the UI shows as a percentage.
        const int strength = editor.indexOfParam(QStringLiteral("strength"));
        QCOMPARE(editor.valueAt(strength), 1.0);
        QCOMPARE(editor.data(editor.index(strength), NodeEditorModel::TextRole).toString(),
                 QStringLiteral("100 %"));
    }

    void aBlockThatIsNotFlowEqKeepsTheGenericEditor()
    {
        // Clean's amp block is a NAM model and a tone stack: nobody's flowEQ.
        session.loadRigNamed(QStringLiteral("Clean"));
        QVERIFY(QTest::qWaitFor([&] { return session.document().name == QLatin1String("Clean"); }, 10000));
        editor.open(QStringLiteral("7A1C2E4F-0001-4000-8000-000000000001"));
        QVERIFY(editor.isOpen());
        QVERIFY(!editor.isFlowEq());
        QVERIFY(flow()->watched().isEmpty());

        session.loadRigNamed(QStringLiteral("Flow"));
        QVERIFY(QTest::qWaitFor([&] { return session.document().name == QLatin1String("Flow"); }, 10000));
        editor.open(kFlow);
        QVERIFY(editor.isOpen() && editor.isFlowEq());
        QCOMPARE(flow()->watched(), kFlow);
    }

    void theEngineDrawsTheCurve()
    {
        QVERIFY(editor.isOpen() && editor.isFlowEq());
        QTRY_VERIFY_WITH_TIMEOUT(flow()->available(), 10000);
        QVERIFY(flow()->curvePath().startsWith(u'M'));
        QCOMPARE(flow()->curvePath().count(u'L'), FlowEqModel::kCurvePoints - 1);
        QCOMPARE(flow()->modelName(), QStringLiteral("vae3d_beta_0.0010"));
        // The decoder's response at the origin peaks at +3 dB, so the axis is the narrowest one.
        QCOMPARE(flow()->dbTop(), FlowEqModel::kMinRangeDb);
        QCOMPARE(flow()->dbBottom(), -FlowEqModel::kMinRangeDb);
    }

    void movingTheLatentRedrawsTheCurve()
    {
        const QString before = flow()->curvePath();
        QCOMPARE(flow()->dbTop(), FlowEqModel::kMinRangeDb);

        // Drag X three quarters of the way: -2 + 0.75 * 4 = 1.0, which peaks at +6.5 dB and so
        // needs the axis to grow to 12.
        const int x = editor.indexOfParam(QStringLiteral("x"));
        QVERIFY(x >= 0);
        editor.setPosition(x, 0.75);
        editor.release(x);

        QTRY_COMPARE_WITH_TIMEOUT(flow()->dbTop(), 12, 10000);
        QVERIFY(flow()->curvePath() != before);

        // And the engine is running what the panel drew: the document says x = 1.
        const auto documentX = [&] {
            for (const auto &chain : session.document().json.value(QStringLiteral("chains")).toArray())
                for (const auto &node : chain.toObject().value(QStringLiteral("nodes")).toArray())
                    if (node.toObject().value(QStringLiteral("type")).toString() == QLatin1String("nc.eq.flow"))
                        return node.toObject().value(QStringLiteral("x")).toDouble();
            return 100.0;
        };
        QVERIFY(QTest::qWaitFor([&] { return std::abs(documentX() - 1.0) < 1e-6; }, 10000));
    }

    void strengthZeroFlattensTheCurve()
    {
        // Strength scales every band's gain, so the engine's curve is the 0 dB line and the panel
        // draws it across the middle rather than keeping the last shape.
        const int strength = editor.indexOfParam(QStringLiteral("strength"));
        editor.setPosition(strength, 0.0);
        editor.release(strength);

        QTRY_VERIFY_WITH_TIMEOUT(flow()->available() && flow()->curvePath().contains(QStringLiteral(" 118.0")), 10000);
        const QStringList parts = flow()->curvePath().split(u'L');
        for (const QString &part : parts) {
            const QString point = part.startsWith(u'M') ? part.mid(1) : part;
            QCOMPARE(point.section(u' ', 1).trimmed(), QStringLiteral("118.0"));
        }

        // Put it back for the tests after this one.
        editor.setPosition(strength, 1.0);
        editor.release(strength);
        QTRY_VERIFY_WITH_TIMEOUT(flow()->dbTop() > FlowEqModel::kMinRangeDb, 10000);
    }

    void anEditFromSomewhereElseRedrawsIt()
    {
        // A footswitch, ljoss or another client can move the latent point while the panel is open.
        // The knobs follow the document; the curve has to follow too, or the panel draws the EQ it
        // was playing a moment ago.
        const QString before = flow()->curvePath();
        QVERIFY(engine.client()->isReady());
        bool done = false;
        engine.client()->request(QStringLiteral("rig.patch"),
                                 QJsonObject{{QStringLiteral("ops"),
                                              QJsonArray{QJsonObject{{QStringLiteral("op"), QStringLiteral("set_param")},
                                                                     {QStringLiteral("node"), kFlow},
                                                                     {QStringLiteral("param"), QStringLiteral("x")},
                                                                     {QStringLiteral("value"), -1.5}}}}},
                                 [&done](const EngineReply &reply) { done = reply.ok; });
        QVERIFY(QTest::qWaitFor([&] { return done; }, 5000));
        QVERIFY(QTest::qWaitFor([&] { return flow()->curvePath() != before; }, 10000));

        // The knob follows too, once the value it was holding (a control moved here keeps its own
        // value for a moment, NodeEditorModel::kHoldMs) has given way to the document's. That
        // second read is NodeEditorModel's delayed one: without it the edit would be visible on the
        // curve and stale on the knob until something else changed the document.
        QTRY_COMPARE_WITH_TIMEOUT(editor.valueAt(editor.indexOfParam(QStringLiteral("x"))), -1.5, 5000);

        // And back, for the tests after this one.
        engine.client()->request(QStringLiteral("rig.patch"),
                                 QJsonObject{{QStringLiteral("ops"),
                                              QJsonArray{QJsonObject{{QStringLiteral("op"), QStringLiteral("set_param")},
                                                                     {QStringLiteral("node"), kFlow},
                                                                     {QStringLiteral("param"), QStringLiteral("x")},
                                                                     {QStringLiteral("value"), 0.0}}}}});
        QVERIFY(QTest::qWaitFor([&] { return flow()->dbTop() == FlowEqModel::kMinRangeDb; }, 10000));
    }

    void autoGainSaysWhatItApplies()
    {
        const int autoGain = editor.indexOfParam(QStringLiteral("auto_gain"));
        QVERIFY(autoGain >= 0);
        QVERIFY(!flow()->autoGain());
        editor.choose(autoGain, 1);
        QTRY_VERIFY_WITH_TIMEOUT(flow()->autoGain(), 10000);
        // The null backend has no audio in, so both mean squares are round-off and the gain holds
        // at unity -- which is itself the report: the panel says 0.0 dB rather than inventing one.
        QCOMPARE(flow()->gainText(), QStringLiteral("0.0 dB"));
        editor.choose(autoGain, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!flow()->autoGain(), 10000);
        QVERIFY(flow()->gainText().isEmpty());
    }

    void closingTheEditorStopsWatching()
    {
        editor.close();
        QVERIFY(!editor.isOpen());
        QVERIFY(flow()->watched().isEmpty());
        QVERIFY(!flow()->available());
        QVERIFY(flow()->curvePath().isEmpty());
    }
};

QTEST_MAIN(TestFlowEqLive)
#include "tst_floweq_live.moc"
