#pragma once

#include "EngineClient.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>

namespace rigpa {

// Small edits on their way to the engine as `rig.patch` ops (ljoss's EngineEdits): a knob being
// turned, a bypass, a model swap.
//
// One patch is in flight at a time and patches leave at most every 10 ms. An op waiting to go is
// replaced by a later one with the same key (the same parameter of the same node), so a knob
// turned faster than the engine answers sends fewer values, never stale ones, and its last value
// is always sent. Ops for one chain that are waiting together go as one patch, in the order they
// were first queued.
class EditStream : public QObject
{
    Q_OBJECT

public:
    static constexpr int kIntervalMs = 10;

    explicit EditStream(EngineClient *client, QObject *parent = nullptr);

    // Queues `op` for `chain` (sent as the patch's "chain", which the engine needs when a rig has
    // several). `key` names what the op sets, for coalescing.
    void submit(const QString &chain, const QString &key, const QJsonObject &op);
    void submit(const QString &chain, const QList<QPair<QString, QJsonObject>> &ops);

    bool busy() const { return m_inFlight || !m_pending.isEmpty(); }
    int sentPatches() const { return m_sent; }

    // The address of a set_param on one node: a primitive by its id, a macro by its element when
    // that is unambiguous (the engine ramps a macro parameter only when it is addressed by element,
    // namchain docs/macros.md), else by node id, which the engine applies as a structural ramp.
    static QJsonObject target(const QString &nodeId, const QString &element, bool macro, bool elementIsUnique);

signals:
    // A patch was answered: `kind` is "smooth" or "structural".
    void answered(const QString &kind);
    void refused(const QString &message);

private:
    struct Pending {
        QString chain;
        QString key;
        QJsonObject op;
    };

    void enqueue(const QString &chain, const QString &key, const QJsonObject &op);
    void flush();

    EngineClient *m_client;
    QList<Pending> m_pending;
    bool m_inFlight = false;
    QElapsedTimer m_sinceSend;
    QTimer m_timer;
    int m_sent = 0;
};

} // namespace rigpa
