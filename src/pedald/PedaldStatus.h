#pragma once

#include "PedaldClient.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// Where the footswitches are, as the pedal service says (GET /status, once a second): which bank,
// the running rig's place in it, and the rig the next press reaches. The compact play view shows
// it ("Bank 1 · 3 of 8", "Next: Crunch").
//
// A stopgap: pedald pushes this once it has an event stream (pedal.project
// docs/pedal-owns-state.md, O5), and then this class reads that instead of polling.
class PedaldStatus : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the pedal's status")
    Q_PROPERTY(bool reachable READ reachable NOTIFY changed)
    // 0-based. -1 while unknown.
    Q_PROPERTY(int bank READ bank NOTIFY changed)
    Q_PROPERTY(int bankCount READ bankCount NOTIFY changed)
    // The running rig's place in its bank, 0-based; -1 when it isn't in the bank.
    Q_PROPERTY(int position READ position NOTIFY changed)
    Q_PROPERTY(int size READ size NOTIFY changed)
    Q_PROPERTY(QString next READ next NOTIFY changed)

public:
    static constexpr int kPollMs = 1000;

    explicit PedaldStatus(PedaldClient *pedald, QObject *parent = nullptr);

    void start();
    // One `/status` body, as the service sends it (tests feed these directly).
    void applyStatus(const QJsonObject &status);
    void setUnreachable();

    bool reachable() const { return m_reachable; }
    int bank() const { return m_bank; }
    int bankCount() const { return m_bankCount; }
    int position() const { return m_position; }
    int size() const { return m_size; }
    QString next() const { return m_next; }

signals:
    void changed();

private:
    void poll();

    PedaldClient *m_pedald;
    QTimer m_timer;
    bool m_inFlight = false;
    bool m_reachable = false;
    int m_bank = -1;
    int m_bankCount = 0;
    int m_position = -1;
    int m_size = 0;
    QString m_next;
};

} // namespace rigpa
