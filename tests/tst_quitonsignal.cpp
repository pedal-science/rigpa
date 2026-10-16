// SIGTERM is a clean exit: systemd's stop must not log rigpa as a failed unit (plan V3).
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <csignal>

#include "app/QuitOnSignal.h"

using rigpa::QuitOnSignal;

class TestQuitOnSignal : public QObject
{
    Q_OBJECT

private slots:
    void termAndIntAreCaught()
    {
        QuitOnSignal quit;
        QVERIFY(quit.isInstalled());
        QSignalSpy received(&quit, &QuitOnSignal::received);
        for (const int s : {SIGTERM, SIGINT}) {
            ::raise(s); // without the handler this ends the test process
            QTRY_COMPARE(received.count(), 1);
            QCOMPARE(received.takeFirst().at(0).toInt(), s);
        }
    }

    void secondInstanceDoesNotTakeOver()
    {
        QuitOnSignal first;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("cannot handle SIGTERM"));
        QuitOnSignal second;
        QVERIFY(first.isInstalled());
        QVERIFY(!second.isInstalled());
    }

    // The app itself: started, then sent what `systemctl stop` sends.
    void rigpaExitsZeroOnTerm()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir(dir.path()).mkpath("models");
        QDir(dir.path()).mkpath("irs");
        QFile config(dir.filePath("rigpa.json"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        // Nothing listens on port 1: the clients retry quietly and touch nothing real.
        config.write(QStringLiteral(R"({
            "settings_file": "%1/settings.ini",
            "library": { "models": "%1/models", "irs": "%1/irs" },
            "engine": { "host": "127.0.0.1", "port": 1 },
            "pedald": { "host": "127.0.0.1", "port": 1 }
        })").arg(dir.path()).toUtf8());
        config.close();

        QProcess rigpa;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");
        rigpa.setProcessEnvironment(env);
        rigpa.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        rigpa.start(QStringLiteral(RIGPA_BINARY), {"--config", config.fileName()});
        QVERIFY(rigpa.waitForStarted());
        QTest::qWait(2000); // well into the event loop, with QML loaded
        QCOMPARE(rigpa.state(), QProcess::Running);
        rigpa.terminate(); // SIGTERM
        QVERIFY(rigpa.waitForFinished(10000));
        QCOMPARE(rigpa.exitStatus(), QProcess::NormalExit);
        QCOMPARE(rigpa.exitCode(), 0);
    }
};

QTEST_GUILESS_MAIN(TestQuitOnSignal)
#include "tst_quitonsignal.moc"
