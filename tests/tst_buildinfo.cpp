#include <QtTest>

#include "app/buildinfo.h"

class TestBuildInfo : public QObject
{
    Q_OBJECT
private slots:
    void screenIsTheBarDisplay()
    {
        QCOMPARE(rigpa::kScreenWidth, 1920);
        QCOMPARE(rigpa::kScreenHeight, 480);
    }
};

QTEST_GUILESS_MAIN(TestBuildInfo)
#include "tst_buildinfo.moc"
