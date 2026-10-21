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
                       double levelDb = -18.0, int blocks = 100)
{
    QJsonObject state{{QStringLiteral("a4"), 440.0},
                      {QStringLiteral("blocks"), blocks},
                      {QStringLiteral("valid"), valid},
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
