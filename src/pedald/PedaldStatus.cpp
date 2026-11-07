#include "PedaldStatus.h"

#include <QJsonArray>

namespace rigpa {

PedaldStatus::PedaldStatus(PedaldClient *pedald, QObject *parent) : QObject(parent), m_pedald(pedald)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &PedaldStatus::poll);
}

void PedaldStatus::start()
{
    poll();
    m_timer.start();
}

void PedaldStatus::poll()
{
    if (m_inFlight || !m_pedald->configured())
        return;
    m_inFlight = true;
    m_pedald->get(QStringLiteral("/status"), [this](const PedaldReply &reply) {
        m_inFlight = false;
        if (reply.ok && reply.body.isObject())
            applyStatus(reply.body.object());
        else
            setUnreachable();
    });
}

void PedaldStatus::applyStatus(const QJsonObject &status)
{
    const QJsonObject bank = status.value(QStringLiteral("bank")).toObject();
    const QJsonArray banks = bank.value(QStringLiteral("banks")).toArray();
    const int index = bank.value(QStringLiteral("bank")).toInt(-1);
    const int size = bank.value(QStringLiteral("size")).toInt(0);
    int position = bank.value(QStringLiteral("position")).toInt(-1);
    if (position >= size)
        position = -1;

    // The rig a press reaches is the one the service holds warm for it (the neighbour in the
    // direction last stepped). With warming off (`state` "off": a device with no warm cores, the
    // Mac's null backend among them) `wanted` is not kept up to date, so the next rig up stands in.
    const QJsonObject warm = status.value(QStringLiteral("engine")).toObject()
                                 .value(QStringLiteral("warm_decision")).toObject();
    QString next = warm.value(QStringLiteral("state")).toString() == QLatin1String("off")
                       ? QString() : warm.value(QStringLiteral("wanted")).toString();
    if (next.isEmpty() && size > 1 && position >= 0 && index >= 0 && index < banks.size())
        next = banks.at(index).toArray().at((position + 1) % size).toString();

    const bool same = m_reachable && index == m_bank && int(banks.size()) == m_bankCount
                      && position == m_position && size == m_size && next == m_next;
    m_reachable = true;
    m_bank = index;
    m_bankCount = int(banks.size());
    m_position = position;
    m_size = size;
    m_next = next;
    if (!same)
        emit changed();
}

void PedaldStatus::setUnreachable()
{
    if (!m_reachable && m_bank == -1)
        return;
    m_reachable = false;
    m_bank = -1;
    m_bankCount = 0;
    m_position = -1;
    m_size = 0;
    m_next.clear();
    emit changed();
}

} // namespace rigpa
