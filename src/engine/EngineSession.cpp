#include "EngineSession.h"

#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>

namespace rigpa {

EngineSession::EngineSession(QObject *parent) : QObject(parent)
{
    m_statusTimer.setInterval(kStatusIntervalMs);
    m_levelsTimer.setInterval(kLevelsIntervalMs);
    connect(&m_statusTimer, &QTimer::timeout, this, &EngineSession::pollStatus);
    connect(&m_levelsTimer, &QTimer::timeout, this, &EngineSession::pollLevels);
    connect(&m_client, &EngineClient::stateChanged, this, [this](EngineClient::State state) {
        if (state == EngineClient::State::Ready) {
            m_statusTimer.start();
            if (m_metersEnabled)
                m_levelsTimer.start();
            pollStatus();
        } else {
            m_statusTimer.stop();
            m_levelsTimer.stop();
            m_statusInFlight = m_levelsInFlight = false;
            clearReadings();
        }
        emit connectionChanged();
    });
}

void EngineSession::start(const QString &host, quint16 port, const QString &token, const QString &problem)
{
    m_problem = problem;
    if (!problem.isEmpty() || token.isEmpty()) {
        if (m_problem.isEmpty())
            m_problem = tr("no engine token configured");
        m_client.disconnectFromEngine();
        emit connectionChanged();
        return;
    }
    m_client.connectTo(host, port, token);
    emit connectionChanged();
}

EngineSession::Connection EngineSession::connection() const
{
    if (!m_problem.isEmpty())
        return NotConfigured;
    switch (m_client.state()) {
    case EngineClient::State::Idle:
    case EngineClient::State::Connecting:
    case EngineClient::State::Authenticating:
        return Connecting;
    case EngineClient::State::Ready:
        return Connected;
    case EngineClient::State::Waiting:
        return Retrying;
    case EngineClient::State::Refused:
        return Refused;
    }
    return Connecting;
}

QString EngineSession::connectionText() const
{
    switch (connection()) {
    case NotConfigured:
        return m_problem;
    case Connecting:
        return tr("connecting…");
    case Connected:
        return tr("connected");
    case Retrying:
        return tr("retrying: %1").arg(m_client.lastError());
    case Refused:
        return tr("refused: %1").arg(m_client.lastError());
    }
    return {};
}

void EngineSession::setMetersEnabled(bool on)
{
    if (on == m_metersEnabled)
        return;
    m_metersEnabled = on;
    if (on && m_client.isReady())
        m_levelsTimer.start();
    if (!on) {
        m_levelsTimer.stop();
        m_inputPeaks.clear();
        m_outputPeaks.clear();
        emit levelsChanged();
    }
    emit metersEnabledChanged();
}

void EngineSession::pollStatus()
{
    if (m_statusInFlight)
        return;
    m_statusInFlight = true;
    m_client.request(QStringLiteral("engine.status"), {}, [this](const EngineReply &reply) {
        m_statusInFlight = false;
        if (reply.ok)
            applyStatus(reply.body.value(QStringLiteral("status")).toObject());
    });
}

void EngineSession::pollLevels()
{
    if (m_levelsInFlight || !m_client.hasFeature(QStringLiteral("levels")))
        return;
    m_levelsInFlight = true;
    m_client.request(QStringLiteral("engine.levels"), {}, [this](const EngineReply &reply) {
        m_levelsInFlight = false;
        if (reply.ok && m_metersEnabled)
            applyLevels(reply.body.value(QStringLiteral("levels")).toObject());
    });
}

double EngineSession::recentWorstPercent(const QMap<int, qint64> &before, const QMap<int, qint64> &after)
{
    double worst = -1;
    for (auto it = after.cbegin(); it != after.cend(); ++it)
        if (it.value() > before.value(it.key(), 0))
            worst = std::max(worst, double(it.key()));
    return worst;
}

void EngineSession::applyStatus(const QJsonObject &status)
{
    m_engineState = status.value(QStringLiteral("state")).toString();
    m_health = status.value(QStringLiteral("health")).toString();
    m_xruns = status.value(QStringLiteral("xruns")).toInteger();
    m_sampleRate = status.value(QStringLiteral("sample_rate")).toInt();
    m_periodSize = status.value(QStringLiteral("period_size")).toInt();

    // The most serious issue that isn't merely informational.
    m_issue.clear();
    int rank = 0;
    for (const auto &value : status.value(QStringLiteral("issues")).toArray()) {
        const QJsonObject issue = value.toObject();
        const QString severity = issue.value(QStringLiteral("severity")).toString();
        const int r = severity == QLatin1String("error") ? 2 : severity == QLatin1String("warn") ? 1 : 0;
        if (r > rank) {
            rank = r;
            m_issue = issue.value(QStringLiteral("message")).toString();
        }
    }

    // DSP load: worst_percent is the worst since start, so the screen shows the worst in the
    // blocks since the last reading instead, held over two readings so it doesn't flicker.
    QMap<int, qint64> histogram;
    for (const auto &bucket : status.value(QStringLiteral("render")).toObject()
                                  .value(QStringLiteral("load_histogram")).toArray()) {
        const QJsonArray pair = bucket.toArray();
        histogram.insert(pair.at(0).toInt(), pair.at(1).toInteger());
    }
    if (m_haveHistogram) {
        const double now = recentWorstPercent(m_histogram, histogram);
        m_dspPercent = std::max(now, m_dspPrevious);
        m_dspPrevious = now;
    }
    m_histogram = histogram;
    m_haveHistogram = true;
    emit statusChanged();

    const QString path = status.value(QStringLiteral("rig")).toString();
    QString name = status.value(QStringLiteral("rig_name")).toString();
    if (name.isEmpty() && !path.isEmpty())
        name = QFileInfo(path).completeBaseName();
    const qint64 generation = status.value(QStringLiteral("generation")).toInteger();
    if (path != m_rigPath || name != m_rigName || generation != m_generation) {
        m_rigPath = path;
        m_rigName = name;
        m_generation = generation;
        emit rigChanged();
    }
}

void EngineSession::applyLevels(const QJsonObject &levels)
{
    auto peaks = [](const QJsonObject &side) {
        QVariantList list;
        for (const auto &v : side.value(QStringLiteral("peak")).toArray())
            list.append(std::max(kFloorDb, v.toDouble(kFloorDb)));
        return list;
    };
    const QJsonObject input = levels.value(QStringLiteral("input")).toObject();
    const QJsonObject output = levels.value(QStringLiteral("output")).toObject();
    // An empty read window means "no reading", not silence: keep what is shown (ljoss does too).
    auto anyFrames = [](const QJsonObject &side) {
        for (const auto &f : side.value(QStringLiteral("frames")).toArray())
            if (f.toInteger() > 0)
                return true;
        return false;
    };
    bool changed = false;
    if (anyFrames(input)) { m_inputPeaks = peaks(input); changed = true; }
    if (anyFrames(output)) { m_outputPeaks = peaks(output); changed = true; }
    if (changed)
        emit levelsChanged();
}

void EngineSession::clearReadings()
{
    m_engineState.clear();
    m_health.clear();
    m_issue.clear();
    m_dspPercent = m_dspPrevious = -1;
    m_haveHistogram = false;
    m_histogram.clear();
    m_inputPeaks.clear();
    m_outputPeaks.clear();
    emit statusChanged();
    emit levelsChanged();
    // Not connected means not knowing what plays: say nothing rather than something stale.
    if (!m_rigPath.isEmpty() || !m_rigName.isEmpty() || m_generation != 0) {
        m_rigPath.clear();
        m_rigName.clear();
        m_generation = 0;
        emit rigChanged();
    }
}

} // namespace rigpa
