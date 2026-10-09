// The node editor against a real namchaind: a knob streams smooth set_params (params_applied
// moves, the generation doesn't), bypass is a crossfade on every node of a block, a model is
// swapped from the picker, an option is sent when chosen, and a burst of edits is coalesced.
#include <QJsonArray>
#include <QtTest>

#include "LiveDaemon.h"
#include "rig/NodeEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

namespace {

const QString kAmp = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000001"); // Clean's NAM + tone stack

// A node of the document the session holds now, by type.
QJsonObject nodeOfType(const RigSession &session, const QString &type)
{
    for (const auto &c : session.document().json.value("chains").toArray())
        for (const auto &n : c.toObject().value("nodes").toArray())
            if (n.toObject().value("type").toString() == type)
                return n.toObject();
    return {};
}

int controlFor(const NodeEditorModel &editor, const QString &param)
{
    for (int i = 0; i < editor.rowCount(); ++i)
        if (editor.paramAt(i) == param)
            return i;
    return -1;
}

} // namespace

class TestEditLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    NodeEditorModel editor{&session, &engine, nullptr, nullptr};

    bool load(const QString &rig)
    {
        session.loadRigNamed(rig);
        return QTest::qWaitFor([&] { return session.document().name == rig; }, 10000);
    }

private slots:
    void initTestCase()
    {
        daemon.start({"Clean", "Dual"}, "Clean");
        rigs.setNames({"Clean", "Dual"});
        engine.start("127.0.0.1", daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && !engine.schema().isEmpty(), 15000);
    }

    void knobStreamsWithoutARebuild()
    {
        QVERIFY(load("Clean"));
        editor.open(kAmp);
        QVERIFY(editor.isOpen());
        // NAM model (a picker), then the tone stack's knobs; then Bypass and Close.
        QCOMPARE(editor.paramAt(0), QStringLiteral("model"));
        const int bass = controlFor(editor, "bass");
        QVERIFY(bass > 0);
        QVERIFY(editor.bypassable());
        QCOMPARE(editor.ring()->count(), editor.rowCount() + 2);
        QCOMPARE(editor.valueAt(bass), 1.2);

        QTRY_VERIFY(engine.hasStatus());
        const qint64 generation = engine.generation();
        const qint64 applied = engine.paramsApplied();
        // The rotary: highlight Bass, select it, turn it ten detents (0.36 dB each).
        editor.ring()->setHighlight(bass);
        editor.ring()->setEditing(true);
        editor.step(bass, 10);
        QCOMPARE(editor.valueAt(bass), 4.8);
        QTRY_COMPARE_WITH_TIMEOUT(nodeOfType(session, "fx.amp.tonestack").value("bass").toDouble(), 4.8, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine.paramsApplied() > applied, 5000);
        QCOMPARE(engine.generation(), generation); // ramped in place: nothing was rebuilt
    }

    void doubleTapResetsToTheDefault()
    {
        const int bass = controlFor(editor, "bass");
        editor.resetToDefault(bass);
        QCOMPARE(editor.valueAt(bass), 0.0); // the tone stack's bass defaults to 0 dB
        QTRY_COMPARE_WITH_TIMEOUT(nodeOfType(session, "fx.amp.tonestack").value("bass").toDouble(), 0.0, 5000);
    }

    void bypassTakesTheWholeBlock()
    {
        QVERIFY(!session.isBypassed(kAmp));
        editor.toggleBypass();
        QTRY_VERIFY_WITH_TIMEOUT(session.isBypassed(kAmp), 5000);
        QVERIFY(editor.bypassed());
        QVERIFY(nodeOfType(session, "nc.nam").value("bypass").toBool());
        QVERIFY(nodeOfType(session, "fx.amp.tonestack").value("bypass").toBool());
        // Apply on the ring's Bypass stop does the same.
        editor.ring()->toggled(editor.bypassIndex());
        QTRY_VERIFY_WITH_TIMEOUT(!session.isBypassed(kAmp), 5000);
    }

    void pickerSwapsTheModel()
    {
        QCOMPARE(editor.pickerKind(0), 0);
        QVERIFY(editor.pickerCurrent(0).startsWith("NAM Ampeg VT-40/"));
        editor.pick(0, "A2-Lite");
        QTRY_COMPARE_WITH_TIMEOUT(nodeOfType(session, "nc.nam").value("model").toString(), QStringLiteral("A2-Lite"), 5000);
        QCOMPARE(editor.pickerCurrent(0), QStringLiteral("A2-Lite"));
    }

    void optionIsSentWhenChosen()
    {
        QVERIFY2(load("Dual"), qPrintable(session.message()));
        const QString cut = nodeOfType(session, "fx.filter.cut").value("element").toString();
        editor.open(cut);
        const int slope = controlFor(editor, "low_slope");
        QVERIFY(slope >= 0);
        QCOMPARE(editor.data(editor.index(slope), NodeEditorModel::KindRole).toString(), QStringLiteral("choice"));
        // Apply on a choice moves it on one (it is a "toggle" stop in the ring).
        editor.ring()->toggled(slope);
        QTRY_COMPARE_WITH_TIMEOUT(nodeOfType(session, "fx.filter.cut").value("low_slope").toVariant().toString(),
                                  QStringLiteral("24"), 5000);
    }

    void aBurstIsCoalesced()
    {
        const QString cut = nodeOfType(session, "fx.filter.cut").value("element").toString();
        editor.open(cut);
        const int lowCut = controlFor(editor, "low_cut");
        const int before = session.edits()->sentPatches();
        for (int i = 0; i <= 100; ++i)
            editor.setPosition(lowCut, i / 100.0); // a fast drag, faster than any reply
        editor.release(lowCut);
        QTRY_VERIFY_WITH_TIMEOUT(!session.edits()->busy(), 5000);
        QVERIFY2(session.edits()->sentPatches() - before <= 3,
                 qPrintable(QString::number(session.edits()->sentPatches() - before)));
        // The last value always arrives.
        QTRY_COMPARE_WITH_TIMEOUT(nodeOfType(session, "fx.filter.cut").value("low_cut").toDouble(), 2000.0, 5000);
    }

    void aSectionHasNothingToAdjustYet()
    {
        const QString split = nodeOfType(session, "nc.split").value("element").toString();
        editor.open(split);
        QVERIFY(editor.isOpen());
        QCOMPARE(editor.rowCount(), 0);
        QVERIFY(!editor.bypassable());
        QCOMPARE(editor.ring()->count(), 1); // Close
        QVERIFY(!editor.note().isEmpty());
        QSignalSpy closing(&editor, &NodeEditorModel::closeRequested);
        editor.ring()->activated(editor.closeIndex());
        QCOMPARE(closing.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestEditLive)
#include "tst_edit_live.moc"
