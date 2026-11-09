#include "PedaldStatus.h"

#include <QJsonArray>

namespace rigpa {

PedaldStatus::PedaldStatus(PedaldEvents *events, QObject *parent) : QObject(parent)
{
    if (!events)
        return;
    connect(events, &PedaldEvents::event, this, [this](const QString &kind, const QJsonObject &data) {
        if (kind == QLatin1String("hello")) {
            applyBanks(data.value(QStringLiteral("bank")).toObject());
            applyRunning(data.value(QStringLiteral("running")).toObject());
        } else if (kind == QLatin1String("running")) {
            applyRunning(data);
        } else if (kind == QLatin1String("bank")) {
            applyBanks(data);
        }
    });
    connect(events, &PedaldEvents::connectedChanged, this, [this, events] {
        if (!events->connected())
            setUnreachable();
    });
}

void PedaldStatus::applyRunning(const QJsonObject &running)
{
    const int bank = running.value(QStringLiteral("bank")).toInt(-1);
    const int size = running.value(QStringLiteral("bank_size")).toInt(0);
    int position = running.value(QStringLiteral("position")).toInt(-1);
    if (position >= size)
        position = -1;
    // The service's own word for where the next press goes: the neighbour in the direction last
    // stepped, past any rig the engine can't play. Nothing when the rig isn't in the bank.
    const QString next = position < 0 ? QString() : running.value(QStringLiteral("next")).toString();
    const bool same = m_reachable && bank == m_bank && position == m_position && size == m_size && next == m_next;
    m_reachable = true;
    m_bank = bank;
    m_position = position;
    m_size = size;
    m_next = next;
    if (!same)
        emit changed();
}

void PedaldStatus::applyBanks(const QJsonObject &bank)
{
    QList<QStringList> banks;
    for (const auto &b : bank.value(QStringLiteral("banks")).toArray()) {
        QStringList rigs;
        for (const auto &r : b.toArray())
            rigs.append(r.toString());
        banks.append(rigs);
    }
    if (banks == m_banks && m_reachable)
        return;
    m_banks = banks;
    m_reachable = true;
    emit changed();
}

QVariantList PedaldStatus::banks() const
{
    QVariantList out;
    for (const QStringList &bank : m_banks)
        out.append(QVariant(bank));
    return out;
}

void PedaldStatus::setUnreachable()
{
    if (!m_reachable && m_bank == -1)
        return;
    m_reachable = false;
    m_bank = -1;
    m_position = -1;
    m_size = 0;
    m_next.clear();
    m_banks.clear();
    emit changed();
}

} // namespace rigpa
