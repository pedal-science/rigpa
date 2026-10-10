#pragma once

// A real namchaind for the live tests: the null backend, TCP on a port the kernel chose, the
// fixture rigs from dev/rigs, and namchain's generated A2-Lite model as the whole library. Skips
// the test case when namchain isn't built beside this repo.
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

struct LiveDaemon {
    QTemporaryDir dir;
    QProcess process;
    QProcess pedald;
    quint16 port = 0;
    quint16 pedaldPort = 0;
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

    // pedald too, from ../pedal.project's source (RIGPA_PEDAL_ROOTFS), as the dev loop runs it:
    // the engine's configuration written out for it to read, and a Pico link that just retries.
    void startPedald()
    {
        const QString rootfs = qEnvironmentVariable("RIGPA_PEDAL_ROOTFS");
        if (rootfs.isEmpty() || !QFile::exists(rootfs + "/usr/local/sbin/pedald"))
            QSKIP("no pedald (RIGPA_PEDAL_ROOTFS)");
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            pedaldPort = probe.serverPort();
        }
        const auto write = [&](const QString &name, const QJsonObject &json) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(json).toJson());
        };
        write("namchaind.json", {{"backend", "null"}, {"rigs_root", dir.filePath("rigs")},
                                 {"models_root", dir.filePath("lib")}, {"irs_root", dir.filePath("lib")},
                                 {"socket", "d.sock"}, {"tcp_listen", true}, {"tcp_address", "127.0.0.1"},
                                 {"tcp_port", port}, {"tcp_token_file", dir.filePath("token")}});
        QDir().mkpath(dir.filePath("state"));
        write("pedald.json", {{"uart_device", dir.filePath("no-pico")}, {"http_host", "127.0.0.1"},
                              {"http_port", pedaldPort}, {"state_root", dir.filePath("state")}});
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PEDALD_LIB", rootfs + "/usr/local/lib");
        pedald.setProcessEnvironment(env);
        pedald.setWorkingDirectory(dir.path());
        pedald.setProcessChannelMode(QProcess::MergedChannels);
        pedald.start("python3", {rootfs + "/usr/local/sbin/pedald", "run", "--config", dir.filePath("pedald.json"),
                                 "--engine-config", dir.filePath("namchaind.json")});
        QVERIFY(pedald.waitForStarted());
    }

    ~LiveDaemon()
    {
        for (QProcess *p : {&pedald, &process}) {
            if (p->state() != QProcess::NotRunning) {
                p->terminate();
                if (!p->waitForFinished(3000))
                    p->kill();
            }
        }
    }
};
