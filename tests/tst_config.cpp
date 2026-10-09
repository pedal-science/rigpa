#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "app/Config.h"

using rigpa::Config;

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void missingFileIsAllDefaults()
    {
        QTemporaryDir dir;
        Config config;
        QString error;
        QVERIFY(config.load(dir.filePath("nope.json"), &error));
        QCOMPARE(config.rotation(), 0);
        QCOMPARE(config.scale(), 0.0); // fit the screen
        QVERIFY(!config.settingsFile().isEmpty());
    }

    void readsDisplayAndSettings()
    {
        Config config;
        QString error;
        const auto root = QJsonDocument::fromJson(R"({"settings_file": "/x/s.ini",
            "display": {"rotation": 90, "scale": 0.75}, "dev": {"rotary_invert": true}})").object();
        QVERIFY2(config.loadJson(root, &error), qPrintable(error));
        QCOMPARE(config.rotation(), 90);
        QCOMPARE(config.scale(), 0.75);
        QCOMPARE(config.settingsFile(), QStringLiteral("/x/s.ini"));
        QVERIFY(config.rotaryInverted());
    }

    void refusesWhatItCannotUse()
    {
        Config config;
        QString error;
        QVERIFY(!config.loadJson(QJsonDocument::fromJson(R"({"display": {"rotation": 45}})").object(), &error));
        QVERIFY(error.contains("rotation"));

        QTemporaryDir dir;
        QFile bad(dir.filePath("bad.json"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{ not json");
        bad.close();
        QVERIFY(!config.load(bad.fileName(), &error));
        QVERIFY(error.contains("bad.json"));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
