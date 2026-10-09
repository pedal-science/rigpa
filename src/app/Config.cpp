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
    // The engine: its own config file first (one home per fact, as pedald reads it), then any
    // explicit values. On the pedal the engine's file is the default.
    QString engineConfig = root.value(QStringLiteral("engine_config")).toString();
#ifdef Q_OS_LINUX
    if (!root.contains(QStringLiteral("engine_config")) && !root.contains(QStringLiteral("engine")))
        engineConfig = QStringLiteral("/etc/namchain/namchaind.json");
#endif
    if (!engineConfig.isEmpty()) {
        QFile file(engineConfig);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("cannot read engine_config %1: %2").arg(engineConfig, file.errorString());
            return false;
        }
        const QJsonObject engine = QJsonDocument::fromJson(file.readAll()).object();
        if (engine.contains(QStringLiteral("tcp_port")))
            m_enginePort = quint16(engine.value(QStringLiteral("tcp_port")).toInt());
        m_engineTokenFile = engine.value(QStringLiteral("tcp_token_file")).toString();
        m_modelsRoot = engine.value(QStringLiteral("models_root")).toString();
        m_irsRoot = engine.value(QStringLiteral("irs_root")).toString();
        // The daemon may listen on 0.0.0.0; rigpa is on the same host either way.
        m_engineHost = QStringLiteral("127.0.0.1");
    }
    const QJsonObject engine = root.value(QStringLiteral("engine")).toObject();
    if (engine.contains(QStringLiteral("host")))
        m_engineHost = engine.value(QStringLiteral("host")).toString();
    if (engine.contains(QStringLiteral("port"))) {
        const int port = engine.value(QStringLiteral("port")).toInt();
        if (port <= 0 || port > 65535) {
            *error = QStringLiteral("engine.port must be 1-65535");
            return false;
        }
        m_enginePort = quint16(port);
    }
    if (engine.contains(QStringLiteral("token_file")))
        m_engineTokenFile = engine.value(QStringLiteral("token_file")).toString();

    QString pedaldConfig = root.value(QStringLiteral("pedald_config")).toString();
#ifdef Q_OS_LINUX
    if (pedaldConfig.isEmpty() && !root.contains(QStringLiteral("pedald"))
        && QFile::exists(QStringLiteral("/etc/pedal/pedald.json")))
        pedaldConfig = QStringLiteral("/etc/pedal/pedald.json");
#endif
    if (!pedaldConfig.isEmpty()) {
        QFile file(pedaldConfig);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("cannot read pedald_config %1: %2").arg(pedaldConfig, file.errorString());
            return false;
        }
        const QJsonObject pedald = QJsonDocument::fromJson(file.readAll()).object();
        if (pedald.contains(QStringLiteral("http_port")))
            m_pedaldPort = quint16(pedald.value(QStringLiteral("http_port")).toInt());
    }
    const QJsonObject pedald = root.value(QStringLiteral("pedald")).toObject();
    if (pedald.contains(QStringLiteral("host")))
        m_pedaldHost = pedald.value(QStringLiteral("host")).toString();
    if (pedald.contains(QStringLiteral("port")))
        m_pedaldPort = quint16(pedald.value(QStringLiteral("port")).toInt());

    const QJsonObject library = root.value(QStringLiteral("library")).toObject();
    if (library.contains(QStringLiteral("models")))
        m_modelsRoot = QDir::cleanPath(library.value(QStringLiteral("models")).toString());
    if (library.contains(QStringLiteral("irs")))
        m_irsRoot = QDir::cleanPath(library.value(QStringLiteral("irs")).toString());

    m_rotaryInverted = root.value(QStringLiteral("dev")).toObject()
                           .value(QStringLiteral("rotary_invert")).toBool(false);
    return true;
}

QString Config::engineToken(QString *problem) const
{
    QString path = m_engineTokenFile;
    const QString credentials = qEnvironmentVariable("CREDENTIALS_DIRECTORY");
    if (!credentials.isEmpty() && QFile::exists(QDir(credentials).filePath(QStringLiteral("token"))))
        path = QDir(credentials).filePath(QStringLiteral("token"));
    if (path.isEmpty()) {
        *problem = QStringLiteral("no engine token file configured");
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *problem = QStringLiteral("cannot read the engine token %1: %2").arg(path, file.errorString());
        return {};
    }
    // The first line, trimmed: the daemon's own rule (namchain src/daemon/config.cpp).
    const QString token = QString::fromUtf8(file.readLine()).trimmed();
    if (token.size() < 16) {
        *problem = QStringLiteral("the engine token in %1 is shorter than 16 characters").arg(path);
        return {};
    }
    return token;
}

QString Config::settingsFile() const
{
    if (!m_settingsFile.isEmpty())
        return m_settingsFile;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("settings.ini"));
}

} // namespace rigpa
