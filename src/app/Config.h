#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// rigpa.json: what this installation is (docs/plan.md, "Config and settings"). Later milestones
// add the engine and pedald sections; this one knows the display and where settings live.
//
//   {
//     "engine_config": "/etc/namchain/namchaind.json",   // its tcp_port and tcp_token_file (vip)
//     "engine": { "host": "127.0.0.1", "port": 47810, "token_file": "..." },  // overrides (Mac)
//     "library": { "models": "...", "irs": "..." },   // overrides; else engine_config's roots
//     "pedald_config": "/etc/pedal/pedald.json",       // its http_port (vip)
//     "pedald": { "host": "127.0.0.1", "port": 47820 },  // overrides
//     "settings_file": "/var/lib/rigpa/settings.ini",
//     "display": { "rotation": 90, "scale": 1.0 },   // scale 0 (or absent): fit the screen
//     "dev": { "rotary_invert": false }
//   }
class Config : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the config")
    Q_PROPERTY(int rotation READ rotation CONSTANT)
    Q_PROPERTY(double scale READ scale CONSTANT)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    explicit Config(QObject *parent = nullptr);

    // Where to look when no path is given: $RIGPA_CONFIG, then the platform's config directory.
    static QString defaultPath();

    // Reads the file. A missing file is not an error: every value has a default. A file that
    // exists but cannot be used is, and `error` says why.
    bool load(const QString &path, QString *error);
    bool loadJson(const QJsonObject &root, QString *error);

    QString path() const { return m_path; }
    QString settingsFile() const;
    int rotation() const { return m_rotation; }
    double scale() const { return m_scale; }
    bool rotaryInverted() const { return m_rotaryInverted; }

    // namchaind's address, and its token: from $CREDENTIALS_DIRECTORY/token when systemd gave one
    // (LoadCredential, as pedald does), else the token file. An empty token comes with `problem`.
    QString engineHost() const { return m_engineHost; }
    quint16 enginePort() const { return m_enginePort; }
    QString engineTokenFile() const { return m_engineTokenFile; }
    QString engineToken(QString *problem) const;

    // The NAM and IR roots rigpa reads (read-only): namchaind's own, or the overrides.
    QString modelsRoot() const { return m_modelsRoot; }
    QString irsRoot() const { return m_irsRoot; }

    // pedald's HTTP API (the rig library). It takes the engine's token.
    QString pedaldHost() const { return m_pedaldHost; }
    quint16 pedaldPort() const { return m_pedaldPort; }
    QJsonObject root() const { return m_root; }

private:
    QString m_path;
    QJsonObject m_root;
    QString m_settingsFile;
    int m_rotation = 0;
    double m_scale = 0;
    bool m_rotaryInverted = false;
    QString m_engineHost = QStringLiteral("127.0.0.1");
    quint16 m_enginePort = 47810;
    QString m_engineTokenFile;
    QString m_modelsRoot;
    QString m_irsRoot;
    QString m_pedaldHost = QStringLiteral("127.0.0.1");
    quint16 m_pedaldPort = 47820;
};

} // namespace rigpa
