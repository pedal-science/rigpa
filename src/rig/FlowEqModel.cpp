#include "FlowEqModel.h"

#include "engine/EngineClient.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace rigpa {

namespace {

QString format_db(double db)
{
    return (db > 0.005 ? QStringLiteral("+") : QString()) + QString::number(db, 'f', 1) + QStringLiteral(" dB");
}

} // namespace

FlowEqModel::FlowEqModel(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_quietTicks <= 0) {
            m_timer.stop();
            return;
        }
        --m_quietTicks;
        request();
    });
}

void FlowEqModel::watch(const QString &element)
{
    if (element == m_element)
        return;
    m_element = element;
    m_timer.stop();
    m_quietTicks = 0;
    m_inFlight = false; // any reply still in flight names the node, and the next line ignores it
    const bool had = m_available;
    m_available = false;
    m_points.clear();
    m_path.clear();
    m_fill.clear();
    m_gainText.clear();
    m_autoGain = false;
    m_model.clear();
    m_dbTop = kMinRangeDb;
    m_dbBottom = -kMinRangeDb;
    if (had)
        emit changed();
    if (m_element.isEmpty())
        return;
    m_quietTicks = kQuietMs / kPollMs;
    m_timer.start();
    request();
}

void FlowEqModel::noteEdit()
{
    if (m_element.isEmpty())
        return;
    m_quietTicks = kQuietMs / kPollMs;
    if (!m_timer.isActive())
        m_timer.start();
    request();
}

void FlowEqModel::request()
{
    if (m_client == nullptr || m_element.isEmpty() || m_inFlight)
        return;
    // A control streams at about 100 Hz, so a request per edit would be ten times more than the
    // screen can show. One read per 50 ms is the most the panel draws.
    if (m_sinceRequest.isValid() && m_sinceRequest.elapsed() < 50)
        return;
    m_inFlight = true;
    m_sinceRequest.restart();
    m_client->request(QStringLiteral("rig.state"), {}, [this](const EngineReply &reply) {
        m_inFlight = false;
        if (reply.ok)
            applyReply(reply.body);
    });
}

void FlowEqModel::applyReply(const QJsonObject &body)
{
    if (m_element.isEmpty())
        return;
    for (const QJsonValue &value : body.value(QStringLiteral("nodes")).toArray()) {
        const QJsonObject node = value.toObject();
        if (node.value(QStringLiteral("element")).toString() != m_element)
            continue;
        const QJsonObject state = node.value(QStringLiteral("state")).toObject();
        const QJsonArray points = state.value(QStringLiteral("curve")).toObject()
                                      .value(QStringLiteral("points")).toArray();
        const bool available = points.size() >= 2;
        const bool autoGain = state.value(QStringLiteral("auto_gain")).toBool();
        const QString gain = autoGain ? format_db(state.value(QStringLiteral("gain_db")).toDouble()) : QString();
        const QString model = state.value(QStringLiteral("model")).toString();

        bool samePoints = available && m_points.size() == points.size();
        for (int i = 0; samePoints && i < points.size(); ++i)
            samePoints = std::abs(m_points.at(i).toDouble() - points.at(i).toDouble()) < 0.005;
        if (available == m_available && autoGain == m_autoGain && gain == m_gainText && model == m_model
            && samePoints)
            return; // nothing the panel would draw differently

        m_available = available;
        m_autoGain = autoGain;
        m_gainText = gain;
        m_model = model;
        if (available) {
            QVariantList list;
            list.reserve(points.size());
            for (const QJsonValue &point : points)
                list.append(point.toDouble());
            setCurve(list);
        } else {
            m_points.clear();
            m_path.clear();
            m_fill.clear();
        }
        emit changed();
        return;
    }

    // The running plan has no such node any more (its chain was swapped, or the rig changed under
    // the panel): stop claiming to know what it is doing.
    if (m_available || !m_path.isEmpty()) {
        m_available = false;
        m_points.clear();
        m_path.clear();
        m_fill.clear();
        m_gainText.clear();
        emit changed();
    }
}

void FlowEqModel::setCurve(const QVariantList &points)
{
    // A symmetric, "nice" range: the smallest multiple of 6 dB that holds the whole curve, so a
    // gentle curve fills the plot instead of hugging a flat line in the middle of a wide one.
    double peak = 0.0;
    for (const QVariant &point : points)
        peak = std::max(peak, std::abs(point.toDouble()));
    const int bound = std::clamp(int(std::ceil(peak / kMinRangeDb)) * kMinRangeDb, kMinRangeDb, kMaxRangeDb);
    m_dbTop = bound;
    m_dbBottom = -bound;
    m_points = points;

    const double width = kCurveWidth;
    const double height = kCurveHeight;
    const double span = double(m_dbTop - m_dbBottom);
    QString path;
    path.reserve(points.size() * 14 + 8);
    for (int i = 0; i < points.size(); ++i) {
        const double x = width * double(i) / double(points.size() - 1);
        const double db = std::clamp(points.at(i).toDouble(), double(m_dbBottom), double(m_dbTop));
        const double y = (double(m_dbTop) - db) / span * height;
        path += (i == 0 ? QStringLiteral("M") : QStringLiteral("L")) + QString::number(x, 'f', 1) + u' '
                + QString::number(y, 'f', 1);
    }
    m_path = path;
    // The same line closed down to the bottom of the plot, so the panel can shade under it.
    m_fill = path + QStringLiteral("L") + QString::number(width, 'f', 0) + u' '
             + QString::number(height, 'f', 0) + QStringLiteral("L0 ") + QString::number(height, 'f', 0)
             + QStringLiteral("Z");
}

} // namespace rigpa
