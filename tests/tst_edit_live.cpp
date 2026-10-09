// The node editor against a real namchaind: a knob streams smooth set_params (params_applied
// moves, the generation doesn't), bypass is a crossfade on every node of a block, a model is
// swapped from the picker, an option is sent when chosen, and a burst of edits is coalesced.
#include <QJsonArray>
#include <QtTest>

#include "LiveDaemon.h"
#include "library/Library.h"
#include "rig/NodeEditorModel.h"
#include "rig/PaletteModel.h"
#include "rig/RigEditorModel.h"
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
        QCOMPARE(editor.ring()->count(), editor.rowCount() + 3); // Bypass, Close, Remove
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

    // Dual's section: the number of lanes, and each lane's level, mute and polarity (a gain node
    // at the lane's end, as ljoss writes it); then undo takes it all back.
    void sectionLanesAndUndo()
    {
        const QString split = nodeOfType(session, "nc.split").value("element").toString();
        editor.open(split);
        QVERIFY(editor.isOpen());
        QVERIFY(!editor.bypassable());
        QCOMPARE(editor.rowCount(), 1 + 2 * 3); // Lanes; then Level, Mute, Invert for each lane
        QCOMPARE(editor.ring()->count(), editor.rowCount() + 2); // Close, Remove
        QCOMPARE(editor.paramAt(0), QStringLiteral("lanes"));
        const auto section = [&] { return session.findElement(split); };

        editor.choose(0, 1); // "3"
        QTRY_COMPARE_WITH_TIMEOUT(section()->lanes.size(), 3, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        QCOMPARE(nodeOfType(session, "nc.split").value("outputs").toInt(), 3);
        QCOMPARE(editor.rowCount(), 1 + 3 * 3);

        // Lane 1 to -6 dB: it has no gain node yet, so the first value adds one.
        editor.setPosition(1, (-6.0 + 40) / 52);
        editor.release(1);
        QTRY_VERIFY_WITH_TIMEOUT(section() && !section()->laneGains.value(0).id.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        QVERIFY(qAbs(edits::laneGainOf(*section(), 0).levelDb + 6) < 0.01);
        // Lane 2 muted.
        editor.choose(5, 1);
        QTRY_VERIFY_WITH_TIMEOUT(section() && edits::laneGainOf(*section(), 1).mute, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);

        QVERIFY(session.canUndo());
        session.undo(); // the mute
        session.undo(); // the level
        session.undo(); // the third lane
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        session.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(section()->lanes.size(), 2, 5000);
        QVERIFY(section()->laneGains.value(0).id.isEmpty());
        QVERIFY(session.canRedo());
        session.redo();
        QTRY_COMPARE_WITH_TIMEOUT(section()->lanes.size(), 3, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
    }

    void removeFromTheEditor()
    {
        const QString cut = nodeOfType(session, "fx.filter.cut").value("element").toString();
        editor.open(cut);
        QSignalSpy closing(&editor, &NodeEditorModel::openChanged);
        editor.ring()->activated(editor.removeIndex());
        QTRY_VERIFY_WITH_TIMEOUT(!session.findElement(cut), 5000);
        QVERIFY(!editor.isOpen()); // its block went, so the editor closed
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        session.undo();
        QTRY_VERIFY_WITH_TIMEOUT(session.findElement(cut), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
    }

    // The palette's Amp: a NAM model (the library's first) and a tone stack, as one block.
    void paletteAddsAnAmp()
    {
        Library library;
        library.setRoots(daemon.dir.filePath("lib"), daemon.dir.filePath("lib"));
        QSignalSpy scanned(&library, &Library::scanned);
        library.rescan();
        QVERIFY(scanned.wait(5000));
        PaletteModel palette(&engine, &library);
        QVERIFY(palette.rowCount() > 2);
        const int amp = palette.indexOfType("amp");
        const auto made = palette.make(amp);
        QVERIFY(made);
        QCOMPARE(made->nodes.size(), 2);
        QCOMPARE(made->nodes[0].json.value("model").toString(), QStringLiteral("A2-Lite"));
        QCOMPARE(made->nodes[1].id, made->element + "-tone");
        // A palette for every engine type it lists.
        for (int r = 0; r < palette.rowCount(); ++r)
            QVERIFY2(palette.make(r), qPrintable(palette.data(palette.index(r), PaletteModel::NameRole).toString()));

        // Dual can't take another amp: over the budget, refused, and the rig stays as it was.
        RigDocument doc = session.document();
        QVERIFY(edits::insert(doc, *made, {0, {}, 0}));
        session.applyDocument(doc);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        QVERIFY2(session.message().startsWith("Too much for the pedal"), qPrintable(session.message()));
        QTRY_VERIFY_WITH_TIMEOUT(!session.findElement(made->element), 5000);

        // Clean can.
        QVERIFY(load("Clean"));
        const qint64 generation = engine.generation();
        doc = session.document();
        QVERIFY(edits::insert(doc, *made, {0, {}, 0}));
        session.applyDocument(doc);
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        QVERIFY2(session.message().isEmpty(), qPrintable(session.message()));
        QTRY_VERIFY_WITH_TIMEOUT(engine.generation() > generation, 5000);
        session.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(session.document().chains[0].series.first().element, made->element, 5000);
        QCOMPARE(session.document().chains[0].series.first().nodes.size(), 2);
        session.undo();
        QTRY_VERIFY_WITH_TIMEOUT(!session.findElement(made->element), 5000);
    }

    // A drag in the rig editor: the cab lifted and dropped before the amp. The preview is the
    // document the drop applies, and the engine ends up running it.
    void dragAndDropReordersTheChain()
    {
        QVERIFY(load("Clean"));
        RigEditorModel view(&session, nullptr);
        view.setAvailableWidth(1200);
        const QString cab = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000002");
        QVERIFY(view.lift(cab));
        QVERIFY(view.dragging());
        // Just right of In, on its row: slot 0.
        const QRectF amp = view.itemFor(kAmp)->rect;
        view.dragTo(QPointF(amp.left() + 10, amp.center().y()));
        QCOMPARE(view.dropPlace(), (edits::Place{0, {}, 0}));
        // The preview already has the cab first: the amp's tile has stepped right.
        QVERIFY(view.itemFor(kAmp)->rect.left() > amp.left());
        QCOMPARE(view.itemFor(cab)->lift, 1);

        const qint64 generation = engine.generation();
        view.drop();
        QVERIFY(!view.dragging());
        QCOMPARE(session.document().chains[0].series.first().element, cab); // shown at once
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine.generation() > generation, 5000);
        session.refresh();
        QTest::qWait(300);
        QCOMPARE(session.document().chains[0].series.first().element, cab); // as the engine has it
        QVERIFY(session.document().chains[0].editable);
    }

    void dropOnTrashRemoves()
    {
        RigEditorModel view(&session, nullptr);
        view.setAvailableWidth(1200);
        QVERIFY(view.lift(kAmp));
        view.dropOnTrash();
        QTRY_VERIFY_WITH_TIMEOUT(!session.applying(), 5000);
        session.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!session.findElement(kAmp), 5000);
        QCOMPARE(session.document().chains[0].series.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestEditLive)
#include "tst_edit_live.moc"
