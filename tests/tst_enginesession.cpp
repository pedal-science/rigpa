#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

#include "engine/EngineSession.h"

using rigpa::EngineSession;

namespace {
QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }
}

class TestEngineSession : public QObject
{
    Q_OBJECT

private slots:
    void recentWorstIsFromNewBlocksOnly()
    {
        QMap<int, qint64> before{{5, 100}, {225, 1}}; // the startup spike stays in the history
        QMap<int, qint64> after{{5, 180}, {12, 3}, {225, 1}};
        QCOMPARE(EngineSession::recentWorstPercent(before, after), 12.0);
        QCOMPARE(EngineSession::recentWorstPercent(after, after), -1.0); // nothing rendered
    }

    void statusFieldsAndDspHold()
    {
        EngineSession session;
        session.applyStatus(json(R"({"state":"running","health":"ok","xruns":2,"sample_rate":48000,
            "period_size":64,"render":{"load_histogram":[[3,10]]},"rig":"/r/rigs/Clean.json",
            "rig_name":"Clean","generation":1,"issues":[{"severity":"info","message":"fyi"}]})"));
        QCOMPARE(session.engineState(), QStringLiteral("running"));
        QCOMPARE(session.xruns(), 2);
        QCOMPARE(session.dspPercent(), -1.0); // one reading: no window yet
        QCOMPARE(session.issue(), QString());  // info isn't shown
        QCOMPARE(session.rigName(), QStringLiteral("Clean"));

        session.applyStatus(json(R"({"render":{"load_histogram":[[3,20],[40,1]]},"issues":[
            {"severity":"warn","message":"careful"},{"severity":"error","message":"broken"}]})"));
        QCOMPARE(session.dspPercent(), 40.0);
        QCOMPARE(session.issue(), QStringLiteral("broken"));
        // A quieter window still shows the previous one's peak, once.
        session.applyStatus(json(R"({"render":{"load_histogram":[[3,30],[40,1]]}})"));
        QCOMPARE(session.dspPercent(), 40.0);
        session.applyStatus(json(R"({"render":{"load_histogram":[[3,40],[40,1]]}})"));
        QCOMPARE(session.dspPercent(), 3.0);
    }

    void unnamedRigTakesItsFileName()
    {
        EngineSession session;
        QSignalSpy changed(&session, &EngineSession::rigChanged);
        session.applyStatus(json(R"({"rig":"/x/rigs/guitar-chain.json","rig_name":"","generation":3})"));
        QCOMPARE(session.rigName(), QStringLiteral("guitar-chain"));
        QVERIFY(session.hasRig());
        session.applyStatus(json(R"({"rig":"/x/rigs/guitar-chain.json","rig_name":"","generation":3})"));
        QCOMPARE(changed.count(), 1);
    }

    void levelsFloorAndEmptyWindows()
    {
        EngineSession session;
        session.applyLevels(json(R"({"input":{"peak":[-200,-6.5],"frames":[64,64]},
                                     "output":{"peak":[-1],"frames":[64]}})"));
        QCOMPARE(session.inputPeaks().at(0).toDouble(), EngineSession::kFloorDb);
        QCOMPARE(session.inputPeaks().at(1).toDouble(), -6.5);
        // An empty window means "no reading": the meters keep what they show.
        session.applyLevels(json(R"({"input":{"peak":[-200,-200],"frames":[0,0]},
                                     "output":{"peak":[-200],"frames":[0]}})"));
        QCOMPARE(session.inputPeaks().at(1).toDouble(), -6.5);
        session.setMetersEnabled(false);
        QVERIFY(session.inputPeaks().isEmpty());
    }

    void missingTokenIsReportedNotTried()
    {
        EngineSession session;
        session.start("127.0.0.1", 47810, {}, "no engine token file configured");
        QCOMPARE(session.connection(), EngineSession::NotConfigured);
        QVERIFY(session.connectionText().contains("token"));
    }
};

QTEST_GUILESS_MAIN(TestEngineSession)
#include "tst_enginesession.moc"
