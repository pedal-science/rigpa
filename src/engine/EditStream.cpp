#include "EditStream.h"

#include <QJsonArray>

namespace rigpa {

EditStream::EditStream(EngineClient *client, QObject *parent) : QObject(parent), m_client(client)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &EditStream::flush);
}

QJsonObject EditStream::target(const QString &nodeId, const QString &element, bool macro, bool elementIsUnique)
{
    if (macro && elementIsUnique && !element.isEmpty())
        return {{QStringLiteral("element"), element}};
    return {{QStringLiteral("node"), nodeId}};
}

void EditStream::enqueue(const QString &chain, const QString &key, const QJsonObject &op)
{
    for (Pending &p : m_pending) {
        if (p.chain == chain && p.key == key) {
            p.op = op;
            return;
        }
    }
    m_pending.append({chain, key, op});
}

void EditStream::submit(const QString &chain, const QString &key, const QJsonObject &op)
{
    enqueue(chain, key, op);
    flush();
}

void EditStream::submit(const QString &chain, const QList<QPair<QString, QJsonObject>> &ops)
{
    for (const auto &[key, op] : ops)
        enqueue(chain, key, op);
    flush();
}

void EditStream::flush()
{
    if (m_inFlight || m_pending.isEmpty())
        return;
    if (m_sinceSend.isValid() && m_sinceSend.elapsed() < kIntervalMs) {
        if (!m_timer.isActive())
            m_timer.start(int(kIntervalMs - m_sinceSend.elapsed()));
        return;
    }
    const QString chain = m_pending.first().chain;
    QJsonArray ops;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->chain == chain) {
            ops.append(it->op);
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    QJsonObject params{{QStringLiteral("ops"), ops}};
    if (!chain.isEmpty())
        params.insert(QStringLiteral("chain"), chain);
    m_inFlight = true;
    m_sinceSend.start();
    ++m_sent;
    m_client->request(QStringLiteral("rig.patch"), params, [this](const EngineReply &reply) {
        m_inFlight = false;
        if (reply.ok)
            emit answered(reply.body.value(QStringLiteral("kind")).toString());
        else
            emit refused(reply.message.isEmpty() ? reply.code : reply.message);
        flush();
    });
}

} // namespace rigpa
