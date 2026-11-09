#pragma once

#include "PedaldEvents.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// Where the footswitches are, as the pedal service says: which bank, the running rig's place in
// it, and the rig the next press reaches. The compact play view shows it ("Bank 1 · 3 of 8",
// "Next: Crunch"). It is pushed (pedald's `running` and `bank` events), so nothing is polled.
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
    // The bank's rigs, in order, and all of the banks (the Banks view edits them).
    Q_PROPERTY(QVariantList banks READ banks NOTIFY changed)

public:
    explicit PedaldStatus(PedaldEvents *events, QObject *parent = nullptr);

    // pedald's `running` and `bank` events, and `hello`, which holds both (tests feed these directly).
    void applyRunning(const QJsonObject &running);
    void applyBanks(const QJsonObject &bank);
    void setUnreachable();

    bool reachable() const { return m_reachable; }
    int bank() const { return m_bank; }
    int bankCount() const { return int(m_banks.size()); }
    int position() const { return m_position; }
    int size() const { return m_size; }
    QString next() const { return m_next; }
    QVariantList banks() const;
    QList<QStringList> bankLists() const { return m_banks; }

signals:
    void changed();

private:
    bool m_reachable = false;
    int m_bank = -1;
    int m_position = -1;
    int m_size = 0;
    QString m_next;
    QList<QStringList> m_banks;
};

} // namespace rigpa
