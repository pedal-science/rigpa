#pragma once

#include "app/FocusRing.h"

#include <QAbstractListModel>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class PedaldClient;
class PedaldStatus;
class RigList;

// The pedal's banks, as the footswitches walk them, put in order here (pedal-service.md, `PUT /bank`).
//
// A row is a rig, with its bank. A rig is picked up (apply, or a tap) and moved one place at a time
// (the rotary, or the arrows on it), into the next bank when it passes the end of its own, or dragged
// to where it goes; putting it down saves the banks. A bank holds 8 rigs, and a bank left empty goes.
class BanksModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the banks")
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(int lifted READ lifted NOTIFY liftedChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(rigpa::FocusRing *ring READ ring CONSTANT)

public:
    enum Role { NameRole = Qt::UserRole + 1, BankRole, LiftedRole, RunningRole };
    static constexpr int kMaxRigs = 8;   // pedald's MAX_BANK_RIGS

    BanksModel(PedaldStatus *status, PedaldClient *pedald, RigList *rigs, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    int lifted() const { return m_lifted; }
    QString problem() const { return m_problem; }
    FocusRing *ring() { return &m_ring; }
    QList<QStringList> banks() const { return m_banks; }

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    // Pick a rig up, or put it down where it is now (which saves).
    Q_INVOKABLE void lift(int row);
    Q_INVOKABLE void drop();
    // The lifted rig, `steps` places on.
    Q_INVOKABLE void move(int steps);
    // A drag: the rig at `from` put down at `to`, and saved.
    Q_INVOKABLE void moveTo(int from, int to);

    // One rig moved one place: within its bank, or over the end into the next (or a new) bank, or the
    // start into the one before. A full bank it moves into gives up the rig it lands beside. `row`
    // becomes the rig's new row. Pure: the tests drive it directly.
    static QList<QStringList> step(QList<QStringList> banks, int &row, int direction);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void openChanged();
    void liftedChanged();
    void problemChanged();

private:
    void reload();
    void save();
    void setProblem(const QString &problem);
    QPair<int, int> place(int row) const;

    PedaldStatus *m_status;
    PedaldClient *m_pedald;
    RigList *m_rigs;
    FocusRing m_ring;
    QList<QStringList> m_banks;
    QList<QStringList> m_before;   // as they were when the rig was picked up
    bool m_open = false;
    int m_lifted = -1;
    QString m_problem;
};

} // namespace rigpa
