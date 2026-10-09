#pragma once

// A real namchaind for the live tests: the null backend, TCP on a port the kernel chose, the
// fixture rigs from dev/rigs, and namchain's generated A2-Lite model as the whole library. Skips
// the test case when namchain isn't built beside this repo.
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

struct LiveDaemon {
    QTemporaryDir dir;
    QProcess process;
    quint16 port = 0;
    QString token = QStringLiteral("rigpa-live-test-token-0123456789");

    // Skips or fails the calling test case when the daemon can't be used. `rig`, when set, is
    // loaded at start.
    void start(const QStringList &rigs, const QString &rig = {})
    {
        const QString namchaind = qEnvironmentVariable("RIGPA_NAMCHAIND");
        const QString models = qEnvironmentVariable("RIGPA_NAMCHAIN_MODELS");
        if (namchaind.isEmpty() || !QFile::exists(namchaind) || !QFile::exists(models + "/A2-Lite.nam"))
            QSKIP("no built namchaind (RIGPA_NAMCHAIND) or test models (RIGPA_NAMCHAIN_MODELS)");
        QDir().mkpath(dir.filePath("rigs"));
        for (const QString &name : rigs)
            QVERIFY(QFile::copy(QFINDTESTDATA(QStringLiteral("../dev/rigs/%1.json").arg(name)),
                                dir.filePath(QStringLiteral("rigs/%1.json").arg(name))));
        QDir().mkpath(dir.filePath("lib"));
        QFile::copy(models + "/A2-Lite.nam", dir.filePath("lib/A2-Lite.nam"));
        QFile t(dir.filePath("token"));
        QVERIFY(t.open(QIODevice::WriteOnly));
        t.write(token.toUtf8());
        t.close();
        t.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            port = probe.serverPort();
        }
        process.setWorkingDirectory(dir.path());
        process.setProcessChannelMode(QProcess::MergedChannels);
        // Two of each, as the dev loop has: the Dual fixture plays a second chain on channel 2.
        QStringList args{"--backend", "null", "--capture-channels", "2", "--playback-channels", "2",
                         "--no-lock-memory", "--rt-priority", "0", "--socket", "d.sock",
                         "--rigs-root", dir.filePath("rigs"),
                         "--models-root", dir.filePath("lib"), "--irs-root", dir.filePath("lib"),
                         "--tcp-listen", "--tcp-address", "127.0.0.1", "--tcp-port",
                         QString::number(port), "--tcp-token-file", dir.filePath("token")};
        if (!rig.isEmpty())
            args << "--rig" << dir.filePath("rigs/" + rig + ".json");
        process.start(namchaind, args);
        QVERIFY(process.waitForStarted());
    }

    ~LiveDaemon()
    {
        if (process.state() != QProcess::NotRunning) {
            process.terminate();
            if (!process.waitForFinished(3000))
                process.kill();
        }
    }
};
