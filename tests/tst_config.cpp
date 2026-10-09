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

    void readsTheEnginesOwnConfig()
    {
        QTemporaryDir dir;
        QFile engine(dir.filePath("namchaind.json"));
        QVERIFY(engine.open(QIODevice::WriteOnly));
        engine.write(QJsonDocument(QJsonObject{{"tcp_port", 47999}, {"tcp_token_file", dir.filePath("token")},
                                               {"models_root", "/lib/models"}, {"irs_root", "/lib/irs"}}).toJson());
        engine.close();
        QFile token(dir.filePath("token"));
        QVERIFY(token.open(QIODevice::WriteOnly));
        token.write("0123456789abcdef0123\n");
        token.close();

        Config config;
        QString error;
        QVERIFY2(config.loadJson(QJsonObject{{"engine_config", engine.fileName()},
                                             {"library", QJsonObject{{"irs", "/elsewhere/irs/"}}}}, &error),
                 qPrintable(error));
        QCOMPARE(config.enginePort(), quint16(47999));
        QCOMPARE(config.engineHost(), QStringLiteral("127.0.0.1"));
        QCOMPARE(config.modelsRoot(), QStringLiteral("/lib/models"));
        QCOMPARE(config.irsRoot(), QStringLiteral("/elsewhere/irs")); // the override wins
        QString problem;
        QCOMPARE(config.engineToken(&problem), QStringLiteral("0123456789abcdef0123"));
    }

    void aShortTokenIsAProblem()
    {
        QTemporaryDir dir;
        QFile token(dir.filePath("token"));
        QVERIFY(token.open(QIODevice::WriteOnly));
        token.write("short\n");
        token.close();
        Config config;
        QString error, problem;
        QVERIFY(config.loadJson(QJsonObject{{"engine", QJsonObject{{"token_file", token.fileName()}}}}, &error));
        QVERIFY(config.engineToken(&problem).isEmpty());
        QVERIFY(problem.contains("16"));
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
