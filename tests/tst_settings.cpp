#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "app/Settings.h"

using rigpa::Settings;

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("settings.ini"));
        QCOMPARE(settings.rigListWidth(), 288);
        QVERIFY(settings.liveMeters());
    }

    void widthIsClampedAndSnapped()
    {
        QCOMPARE(Settings::clampRigListWidth(10), 192);
        QCOMPARE(Settings::clampRigListWidth(9999), 576);
        QCOMPARE(Settings::clampRigListWidth(300), 304);
        QCOMPARE(Settings::clampRigListWidth(295), 288);
    }

    void rotaryStepsByOneStep()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("settings.ini"));
        settings.stepRigListWidth(2);
        QCOMPARE(settings.rigListWidth(), 320);
        settings.stepRigListWidth(-100);
        QCOMPARE(settings.rigListWidth(), 192);
    }

    void persists()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ini");
        {
            Settings settings(path);
            QSignalSpy changed(&settings, &Settings::rigListWidthChanged);
            settings.setRigListWidth(400);
            settings.setLiveMeters(false);
            QCOMPARE(changed.count(), 1);
        }
        Settings again(path);
        QCOMPARE(again.rigListWidth(), 400);
        QVERIFY(!again.liveMeters());
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
