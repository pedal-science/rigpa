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
    QJsonObject root() const { return m_root; }

private:
    QString m_path;
    QJsonObject m_root;
    QString m_settingsFile;
    int m_rotation = 0;
    double m_scale = 0;
    bool m_rotaryInverted = false;
};

} // namespace rigpa
