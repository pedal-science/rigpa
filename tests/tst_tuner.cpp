// TunerModel without a daemon: the reply `rig.state` carries becomes the note the view draws.
//
// What is checked here is the part that can be wrong without a window: which node in the answer is
// ours, that a reading the engine withholds is shown as no note rather than as the last one, the
// needle's arithmetic at the ends of its scale, the level's fraction, and the rule that a reply whose
// only difference is the block counter does not redraw the screen (docs/spec.md: the screen is static
// when idle).
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include "rig/TunerModel.h"

using namespace rigpa;

namespace {

QJsonObject tunerState(bool valid, const QString &note, int octave, double cents, double hz,
                       double levelDb = -18.0, int blocks = 100, bool muted = true)
{
    QJsonObject state{{QStringLiteral("a4"), 440.0},
                      {QStringLiteral("blocks"), blocks},
                      {QStringLiteral("valid"), valid},
                      {QStringLiteral("mute"), muted},
                      {QStringLiteral("level_db"), levelDb}};
    if (valid) {
        // The engine omits the note keys when it has no note: a client must not see a stale one
        // (namchain docs/tuner.md).
        state.insert(QStringLiteral("note"), note);
        state.insert(QStringLiteral("octave"), octave);
        state.insert(QStringLiteral("midi"), 45);
        state.insert(QStringLiteral("cents"), cents);
        state.insert(QStringLiteral("hz"), hz);
        state.insert(QStringLiteral("steady_hz"), hz);
        state.insert(QStringLiteral("clarity"), 0.93);
        state.insert(QStringLiteral("uncertainty_cents"), 0.08);
        state.insert(QStringLiteral("partials"), 5);
    }
    return state;
}

QJsonObject reply(const QJsonObject &state, const QString &type = QStringLiteral("nc.tuner"),
                  const QString &chain = QStringLiteral("main"))
{
    return {{QStringLiteral("generation"), 1},
            {QStringLiteral("nodes"),
             QJsonArray{QJsonObject{{QStringLiteral("chain"), chain},
                                    {QStringLiteral("node"), QStringLiteral("tuner")},
                                    {QStringLiteral("element"), QString()},
                                    {QStringLiteral("role"), QString()},
                                    {QStringLiteral("type"), type},
                                    {QStringLiteral("state"), state}}}}};
}

/// A body with a flowEQ beside the tuner, as a rig that holds both reports.
QJsonObject withFlowEq(const QJsonObject &tunerState)
{
    QJsonObject flow{{QStringLiteral("model"), QStringLiteral("vae3d_beta_0.0010")},
                     {QStringLiteral("curve"), QJsonObject{{QStringLiteral("points"), QJsonArray{1, 2, 3}}}}};
    QJsonArray nodes = reply(tunerState).value(QStringLiteral("nodes")).toArray();
    nodes.prepend(QJsonObject{{QStringLiteral("chain"), QStringLiteral("main")},
                              {QStringLiteral("node"), QStringLiteral("eq")},
                              {QStringLiteral("element"), QStringLiteral("eq")},
                              {QStringLiteral("type"), QStringLiteral("nc.eq.flow")},
                              {QStringLiteral("state"), flow}});
    return {{QStringLiteral("generation"), 1}, {QStringLiteral("nodes"), nodes}};
}

} // namespace

class TestTuner : public QObject
{
    Q_OBJECT

private slots:
    void startsWaiting()
    {
        TunerModel model;
        QVERIFY(!model.available());
        QVERIFY(!model.running());
        QVERIFY(!model.valid());
        QVERIFY(model.noteText().isEmpty());
        QCOMPARE(model.needleX(), model.barWidth() / 2);
        QVERIFY(model.status().contains(QStringLiteral("Waiting")));
    }

    void aReadingIsTheNoteOnTheScale()
    {
        TunerModel model;
        model.setWatching(true);
        QSignalSpy changed(&model, &TunerModel::changed);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, -1.73, 109.56)));

        QVERIFY(model.available());
        QVERIFY(model.running());
        QVERIFY(model.valid());
        QCOMPARE(model.noteText(), QStringLiteral("A2"));
        QCOMPARE(model.centsText(), QStringLiteral("-1.7"));
        QVERIFY(model.inTune());
        QCOMPARE(model.hzText(), QStringLiteral("109.6 Hz"));
        QVERIFY(model.status().isEmpty());
        QVERIFY(model.a4Text().contains(QStringLiteral("440")));
        // -1.73 cents of the 50 either side is a little left of centre.
        const int mid = model.barWidth() / 2;
        QVERIFY(model.needleX() < mid);
        QVERIFY(model.needleX() > mid - 40);
        QVERIFY(changed.count() > 0);
    }

    void aNoteTheEngineWithholdsIsShownAsNone()
    {
        // The engine omits the note keys when it has no note; the view must not draw the last one.
        TunerModel model;
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("E"), 2, 0.4, 82.4)));
        QCOMPARE(model.noteText(), QStringLiteral("E2"));

        model.applyReply(reply(tunerState(false, QString(), 0, 0.0, 0.0, -74.0)));
        QVERIFY(model.running());
        QVERIFY(!model.valid());
        QVERIFY(model.noteText().isEmpty());
        QVERIFY(model.centsText().isEmpty());
        QVERIFY(model.hzText().isEmpty());
        QCOMPARE(model.status(), QStringLiteral("No note"));
        QCOMPARE(model.needleX(), model.barWidth() / 2);
    }

    void anAbsentNodeMeansTheRigIsNotMuted()
    {
        TunerModel model;
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0)));
        QVERIFY(model.running());

        // A rig the engine runs without a tuner node: this is what a refused publish looks like, and
        // saying so is the whole point -- the player believes the rig is silent.
        model.applyReply({{QStringLiteral("generation"), 2}, {QStringLiteral("nodes"), QJsonArray()}});
        QVERIFY(model.available());
        QVERIFY(!model.running());
        QVERIFY(!model.valid());
        QVERIFY(model.noteText().isEmpty());
        QVERIFY(model.status().contains(QStringLiteral("not muted")));
    }

    void anotherNodesStateIsNotOurs()
    {
        TunerModel model;
        model.setWatching(true);
        model.applyReply(withFlowEq(tunerState(true, QStringLiteral("D"), 3, 2.4, 146.8)));
        QVERIFY(model.valid());
        QCOMPARE(model.noteText(), QStringLiteral("D3"));
        QCOMPARE(model.centsText(), QStringLiteral("+2.4"));
    }

    void theNeedleClampsAtTheEndsOfItsScale()
    {
        TunerModel model;
        model.setWatching(true);
        // The engine's cents is the offset from the nearest note, so it is always inside +/-50; a
        // clamped needle is for a malformed or unexpected value, not for a normal reading.
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 400.0, 460.0)));
        QCOMPARE(model.needleX(), model.barWidth());
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, -400.0, 420.0)));
        QCOMPARE(model.needleX(), 0);
    }

    void theScaleIsTheLayoutsWidth()
    {
        // The 4.3" panel's tuner draws its scale 560 px wide (LayoutSpec): the needle is placed in
        // those pixels, from the middle at rest to either end.
        TunerModel model;
        model.setBarWidth(560);
        QCOMPARE(model.barWidth(), 560);
        QCOMPARE(model.needleX(), 280);
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 25.0, 446.4)));
        QCOMPARE(model.needleX(), 420); // half way to the sharp end
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 400.0, 460.0)));
        QCOMPARE(model.needleX(), 560);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, -400.0, 420.0)));
        QCOMPARE(model.needleX(), 0);
    }

    void anUnchangedReadingDoesNotRedraw()
    {
        // `blocks` moves every block and is not drawn: a note held steady must not repaint the view
        // thirty times a second (docs/spec.md, the screen is static when idle).
        TunerModel model;
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 1.0, 110.0, -18.0, 100)));
        QSignalSpy changed(&model, &TunerModel::changed);
        for (int blocks = 101; blocks < 120; ++blocks)
            model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 1.0, 110.0, -18.0, blocks)));
        QCOMPARE(changed.count(), 0);

        // A note that moves does redraw.
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 3.0, 110.0, -18.0, 120)));
        QCOMPARE(changed.count(), 1);
    }

    void anEmptyLevelIsNoBar()
    {
        TunerModel model;
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0, -60.0)));
        QCOMPARE(model.levelFraction(), 0.0);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0, 0.0)));
        QCOMPARE(model.levelFraction(), 1.0);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0, -30.0)));
        QCOMPARE(model.levelFraction(), 0.5);
    }

    void theSwitchIsTheEngines()
    {
        // The view binds its control to what the engine reports, so a refused patch leaves the two
        // visibly disagreeing rather than silently lying. A flag arrives as 0/1 sometimes, which is
        // the same answer.
        TunerModel model;
        model.setWatching(true);
        QVERIFY(model.muted()); // a tuner comes up muted

        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0, -18.0, 100, false)));
        QVERIFY(!model.muted());

        QJsonObject asNumber = tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0, -18.0, 101, true);
        asNumber.insert(QStringLiteral("mute"), 1);
        model.applyReply(reply(asNumber));
        QVERIFY(model.muted());

        // No node at all: no switch to report either, and the view's status says the rig is not muted.
        model.applyReply({{QStringLiteral("generation"), 1}, {QStringLiteral("nodes"), QJsonArray{}}});
        QVERIFY(model.muted());
        QVERIFY(!model.running());
    }

    void theStrobeDriftsByCentsAndPartial()
    {
        // Ported law (namchain docs/tuner.md, "The strobe"): row k drifts 0.5 * cents * k band
        // periods per second, clamped at 12. A sharp string drifts right, a flat one left, and a
        // perfect one does not move at all.
        TunerModel model;
        model.setWatching(true);
        // Ten cents sharp, a tenth of a second: the rows move 0.5, 1 and 2 periods, so the first
        // lands half way, the second comes back round to the start, and the fourth is clamped to 12
        // periods a second and lands at a fifth.
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 10.0, 442.55)));
        QCOMPARE(model.rowPhase1(), 0.0);
        model.advance(0.1);
        QVERIFY(qAbs(model.rowPhase1() - 0.5) < 0.01);
        QVERIFY(model.rowPhase2() < 0.01 || qAbs(model.rowPhase2() - 1.0) < 0.01);
        QVERIFY(qAbs(model.rowPhase4() - 0.2) < 0.01); // 2 periods/s * 0.5 would be 1.0; the clamp
                                                       // leaves 12 * 0.1 = 1.2, which wraps to 0.2

        // In tune: the rows stand still, which is the reading a player tunes to.
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 0.0, 440.0, -18.0, 101)));
        const qreal held = model.rowPhase1();
        model.advance(0.2);
        QCOMPARE(model.rowPhase1(), held);

        // And with no note there is nothing to drift: the rows settle rather than animate a reading
        // that is not there.
        model.applyReply(reply(tunerState(false, QString(), 0, 0.0, 0.0, -74.0, 102)));
        const qreal settled = model.rowPhase2();
        model.advance(0.2);
        QCOMPARE(model.rowPhase2(), settled);
    }

    void theStrobeClampsAndIsOffWhileNotWatching()
    {
        TunerModel model;
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 50.0, 440.0))); // not watching
        model.advance(0.5);
        QCOMPARE(model.rowPhase1(), 0.0);

        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 4, 50.0, 440.0)));
        // 50 cents is 25 periods a second unclamped, and 12 with the clamp: a quarter of a second is
        // three whole periods, so the row is back at the start. Unclamped it would be at 0.25 --
        // which is what makes this an assertion about the clamp and not about arithmetic.
        model.advance(0.25);
        QVERIFY(qAbs(model.rowPhase1() - 0.0) < 0.01);
        model.advance(1.0 / 24.0);
        QVERIFY(qAbs(model.rowPhase1() - 0.5) < 0.01);
        // A stalled clock must not teleport the bands a whole period.
        model.advance(30.0);
        QVERIFY(model.rowPhase1() >= 0.0 && model.rowPhase1() < 1.0);
    }

    void leavingTheViewForgetsEverything()
    {
        TunerModel model;
        model.setWatching(true);
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0)));
        QVERIFY(model.valid());

        model.setWatching(false);
        QVERIFY(!model.available());
        QVERIFY(!model.running());
        QVERIFY(model.noteText().isEmpty());
        // And a reply that arrives after the view is gone changes nothing.
        model.applyReply(reply(tunerState(true, QStringLiteral("A"), 2, 0.0, 110.0)));
        QVERIFY(!model.available());
    }
};

QTEST_MAIN(TestTuner)
#include "tst_tuner.moc"
