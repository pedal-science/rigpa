// --demo-animate against a real namchaind: it keeps lifting and sweeping nodes for as long as it
// runs, and never changes what the engine plays (the D14 soak runs it beside the render core).
#include <QtTest>

#include "LiveDaemon.h"
#include "app/DemoAnimator.h"
#include "app/Navigator.h"
#include "library/Library.h"
#include "rig/RigEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"

using namespace rigpa;

class TestDemoLive : public QObject
{
    Q_OBJECT
    LiveDaemon daemon;
    PedaldClient pedald;
    RigList rigs{&pedald};
    EngineSession engine;
    RigSession session{&engine, &rigs};
    Library library;
    RigEditorModel editor{&session, &library};
    Navigator nav;

private slots:
    void initTestCase()
    {
        daemon.start({"Dual"}, "Dual");
        engine.start("127.0.0.1", daemon.port, daemon.token);
        QTRY_VERIFY_WITH_TIMEOUT(session.hasDocument() && editor.selectableCount() >= 2, 15000);
    }

    void sweepsWithoutChangingTheRig()
    {
        const qint64 generation = engine.generation();
        const QByteArray document = QJsonDocument(session.document().json).toJson(QJsonDocument::Compact);
        DemoAnimator animator(&editor, &nav);
        animator.start();
        QTRY_VERIFY_WITH_TIMEOUT(animator.sweeps() >= 2, 10000);
        bool sawDrag = false;
        QVERIFY(QTest::qWaitFor([&] { sawDrag = sawDrag || editor.dragging(); return sawDrag; }, 2000));
        QTest::qWait(500); // a status poll or two after the last sweep
        QCOMPARE(engine.generation(), generation);
        QCOMPARE(QJsonDocument(session.document().json).toJson(QJsonDocument::Compact), document);
    }
};

QTEST_GUILESS_MAIN(TestDemoLive)
#include "tst_demo_live.moc"
