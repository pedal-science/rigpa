// FlowEqModel without a daemon: the reply `rig.state` carries becomes the curve the panel draws.
//
// The properties are what the panel binds to, so what is checked here is the part that can be wrong
// without a window: the axis a curve is drawn on, the path it becomes, which replies are ours, and
// the rule that an unchanged state does not redraw the screen (docs/spec.md, the screen is static
// when idle).
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include <cmath>

#include "rig/FlowEqModel.h"

using namespace rigpa;

namespace {

QJsonObject stateReply(const QString &element, const QJsonArray &points, bool autoGain = false,
                       double gainDb = 0.0)
{
    QJsonObject state{{QStringLiteral("model"), QStringLiteral("vae3d_beta_0.0010")},
                      {QStringLiteral("auto_gain"), autoGain},
                      {QStringLiteral("gain_db"), gainDb},
                      {QStringLiteral("curve"), QJsonObject{{QStringLiteral("f_min"), 20.0},
                                                             {QStringLiteral("f_max"), 20000.0},
                                                             {QStringLiteral("points"), points}}}};
    return {{QStringLiteral("generation"), 1},
            {QStringLiteral("nodes"),
             QJsonArray{QJsonObject{{QStringLiteral("chain"), QStringLiteral("main")},
                                    {QStringLiteral("node"), QStringLiteral("eq")},
                                    {QStringLiteral("element"), element},
                                    {QStringLiteral("role"), QString()},
                                    {QStringLiteral("type"), QStringLiteral("nc.eq.flow")},
                                    {QStringLiteral("state"), state}}}}};
}

QJsonArray curveOf(const std::initializer_list<double> &values)
{
    QJsonArray points;
    for (double value : values)
        points.append(value);
    return points;
}

/// The (x, y) pairs a path string is made of: 'M x y' then 'L x y' per point.
QList<QPointF> pointsOf(const QString &path)
{
    QList<QPointF> points;
    for (QString command : path.split(u'L')) {
        if (command.startsWith(u'M'))
            command.remove(0, 1);
        const QStringList parts = command.split(u' ');
        if (parts.size() == 2)
            points.append(QPointF(parts.at(0).toDouble(), parts.at(1).toDouble()));
    }
    return points;
}

} // namespace

class TestFlowEqModel : public QObject
{
    Q_OBJECT

private slots:
    void anUnreadStateKnowsNothing()
    {
        FlowEqModel model;
        QVERIFY(!model.available());
        QVERIFY(model.curvePath().isEmpty());
        QVERIFY(model.fillPath().isEmpty());
        QCOMPARE(model.dbTop(), FlowEqModel::kMinRangeDb);
        QCOMPARE(model.dbBottom(), -FlowEqModel::kMinRangeDb);
        QCOMPARE(model.curveWidth(), FlowEqModel::kCurveWidth);
    }

    void aFlatCurveIsDrawnAcrossTheMiddle()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        QJsonArray points;
        for (int i = 0; i < FlowEqModel::kCurvePoints; ++i)
            points.append(0.0);
        model.applyReply(stateReply(QStringLiteral("eq"), points));

        QVERIFY(model.available());
        // A curve that moves nothing gets the narrowest axis rather than an empty plot.
        QCOMPARE(model.dbTop(), FlowEqModel::kMinRangeDb);
        QCOMPARE(model.dbBottom(), -FlowEqModel::kMinRangeDb);
        const QList<QPointF> drawn = pointsOf(model.curvePath());
        QCOMPARE(drawn.size(), qsizetype(FlowEqModel::kCurvePoints));
        for (const QPointF &point : drawn)
            QCOMPARE(point.y(), FlowEqModel::kCurveHeight / 2.0); // 0 dB is the middle line
        // The fill closes the line down to the bottom of the plot, so the panel can shade under it.
        QVERIFY(model.fillPath().endsWith(u'Z'));
        QVERIFY(model.fillPath().startsWith(model.curvePath()));
    }

    void aLouderCurveWidensTheAxis()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        QJsonArray points;
        for (int i = 0; i < FlowEqModel::kCurvePoints; ++i)
            points.append(i == 40 ? 13.0 : 0.0);
        model.applyReply(stateReply(QStringLiteral("eq"), points));

        // 13 dB needs three 6 dB bands: the axis grows to +/-18 rather than clipping the peak.
        QCOMPARE(model.dbTop(), 18);
        QCOMPARE(model.dbBottom(), -18);
        const QList<QPointF> drawn = pointsOf(model.curvePath());
        QCOMPARE(drawn.size(), qsizetype(FlowEqModel::kCurvePoints));
        // 13 dB on a -18..18 axis is 5/36 of the height from the top.
        // The path is written to a tenth of a pixel, which is what the screen can show.
        const double expected = (18.0 - 13.0) / 36.0 * FlowEqModel::kCurveHeight;
        QVERIFY(std::abs(drawn.at(40).y() - expected) < 0.05);
    }

    void autoGainReportsWhatItApplies()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.0, 0.0}), true, -2.4));
        QVERIFY(model.autoGain());
        QCOMPARE(model.gainText(), QStringLiteral("-2.4 dB"));

        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.0, 0.0}), true, 1.2));
        QCOMPARE(model.gainText(), QStringLiteral("+1.2 dB"));

        // With Auto Gain off the engine's applied gain is Out, and the panel has a knob for that:
        // there is nothing extra to say.
        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.0, 0.0}), false, -2.4));
        QVERIFY(!model.autoGain());
        QVERIFY(model.gainText().isEmpty());
    }

    void theSameStateDoesNotRedrawTheScreen()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        const QJsonArray points = curveOf({0.0, 2.0, 1.0, 0.0});
        QSignalSpy changed(&model, &FlowEqModel::changed);
        model.applyReply(stateReply(QStringLiteral("eq"), points));
        QCOMPARE(changed.count(), 1);
        model.applyReply(stateReply(QStringLiteral("eq"), points));
        QCOMPARE(changed.count(), 1); // nothing moved: no repaint
        // A hundredth of a dB is below what the curve is rounded to, so it is not a change either.
        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.004, 2.0, 1.0, 0.0})));
        QCOMPARE(changed.count(), 1);
    }

    void aReplyWithoutTheNodeStopsClaimingToKnow()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.0, 1.0})));
        QVERIFY(model.available());

        // The running plan no longer has the node (its chain was swapped, or the rig changed): the
        // panel must say it is waiting rather than keep drawing the curve it last saw.
        QSignalSpy changed(&model, &FlowEqModel::changed);
        model.applyReply(QJsonObject{{QStringLiteral("nodes"), QJsonArray{}}});
        QVERIFY(!model.available());
        QVERIFY(model.curvePath().isEmpty());
        QCOMPARE(changed.count(), 1);
    }

    void watchingAnotherElementStopsTheModel()
    {
        FlowEqModel model;
        model.watch(QStringLiteral("eq"));
        model.applyReply(stateReply(QStringLiteral("eq"), curveOf({0.0, 1.0})));
        QVERIFY(model.available());
        QCOMPARE(model.watched(), QStringLiteral("eq"));

        model.watch(QString());
        QVERIFY(model.watched().isEmpty());
        QVERIFY(!model.available());
        model.watch(QString()); // and another empty one changes nothing
        QVERIFY(!model.available());
    }
};

QTEST_MAIN(TestFlowEqModel)
#include "tst_floweq.moc"
