#include "Config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

namespace rigpa {

Config::Config(QObject *parent) : QObject(parent) {}

QString Config::defaultPath()
{
    const QString env = qEnvironmentVariable("RIGPA_CONFIG");
    if (!env.isEmpty())
        return env;
#ifdef Q_OS_LINUX
    return QStringLiteral("/etc/rigpa/rigpa.json");
#else
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("rigpa.json"));
#endif
}

bool Config::load(const QString &path, QString *error)
{
    m_path = path;
    QFile file(path);
    if (!file.exists())
        return loadJson({}, error);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("%1 is not a JSON object: %2").arg(path, parse.errorString());
        return false;
    }
    if (!loadJson(doc.object(), error)) {
        *error = path + QStringLiteral(": ") + *error;
        return false;
    }
    return true;
}

bool Config::loadJson(const QJsonObject &root, QString *error)
{
    m_root = root;
    m_settingsFile = root.value(QStringLiteral("settings_file")).toString();

    const QJsonObject display = root.value(QStringLiteral("display")).toObject();
    m_rotation = display.value(QStringLiteral("rotation")).toInt(0);
    if (m_rotation != 0 && m_rotation != 90 && m_rotation != 180 && m_rotation != 270) {
        *error = QStringLiteral("display.rotation must be 0, 90, 180 or 270, not %1").arg(m_rotation);
        return false;
    }
    m_scale = display.value(QStringLiteral("scale")).toDouble(0);
    if (m_scale < 0 || m_scale > 4) {
        *error = QStringLiteral("display.scale must be between 0 (fit) and 4");
        return false;
    }
    m_rotaryInverted = root.value(QStringLiteral("dev")).toObject()
                           .value(QStringLiteral("rotary_invert")).toBool(false);
    return true;
}

QString Config::settingsFile() const
{
    if (!m_settingsFile.isEmpty())
        return m_settingsFile;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("settings.ini"));
}

} // namespace rigpa
