#include "BanksModel.h"

#include "RigList.h"
#include "pedald/PedaldClient.h"
#include "pedald/PedaldStatus.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rigpa {

BanksModel::BanksModel(PedaldStatus *status, PedaldClient *pedald, RigList *rigs, QObject *parent)
    : QAbstractListModel(parent), m_status(status), m_pedald(pedald), m_rigs(rigs)
{
    connect(m_status, &PedaldStatus::changed, this, [this] {
        if (m_lifted < 0)
            reload();
    });
    if (m_rigs)
        connect(m_rigs, &RigList::runningChanged, this, [this] {
            if (rowCount() > 0)
                emit dataChanged(index(0), index(rowCount() - 1), {RunningRole});
        });
    connect(&m_ring, &FocusRing::stepped, this, [this](int, int delta) { move(delta); });
    connect(&m_ring, &FocusRing::editingChanged, this, [this] {
        if (m_ring.editing() && m_lifted < 0)
            lift(m_ring.highlight());
        else if (!m_ring.editing() && m_lifted >= 0)
            drop();
    });
    reload();
}

void BanksModel::reload()
{
    beginResetModel();
    m_banks = m_status->bankLists();
    endResetModel();
    QStringList kinds;
    for (int i = 0; i < rowCount(); ++i)
        kinds.append(QStringLiteral("value"));
    m_ring.setKinds(kinds);
    m_ring.setCount(int(kinds.size()));
}

void BanksModel::open()
{
    m_open = true;
    setProblem({});
    reload();
    m_ring.setHighlight(0);
    emit openChanged();
}

void BanksModel::close()
{
    if (!m_open)
        return;
    if (m_lifted >= 0)
        drop();
    m_open = false;
    emit openChanged();
}

QPair<int, int> BanksModel::place(int row) const
{
    for (int b = 0; b < m_banks.size(); ++b) {
        if (row < m_banks.at(b).size())
            return {b, row};
        row -= int(m_banks.at(b).size());
    }
    return {-1, -1};
}

void BanksModel::lift(int row)
{
    if (row < 0 || row >= rowCount() || m_lifted == row)
        return;
    if (m_lifted >= 0)
        drop();
    m_before = m_banks;
    m_lifted = row;
    m_ring.setHighlight(row);
    if (!m_ring.editing())
        m_ring.setEditing(true);
    emit dataChanged(index(row), index(row), {LiftedRole});
    emit liftedChanged();
}

void BanksModel::drop()
{
    if (m_lifted < 0)
        return;
    const int row = m_lifted;
    m_lifted = -1;
    if (m_ring.editing())
        m_ring.setEditing(false);
    emit dataChanged(index(row), index(row), {LiftedRole});
    emit liftedChanged();
    if (m_banks != m_before)
        save();
}

QList<QStringList> BanksModel::step(QList<QStringList> banks, int &row, int direction)
{
    int b = 0, at = row;
    while (b < banks.size() && at >= banks.at(b).size()) {
        at -= int(banks.at(b).size());
        ++b;
    }
    if (b >= banks.size() || direction == 0)
        return banks;
    QStringList &bank = banks[b];
    const int to = at + (direction > 0 ? 1 : -1);
    if (to >= 0 && to < bank.size()) {
        bank.swapItemsAt(at, to);
    } else if (direction > 0) {
        if (bank.size() == 1 && b == banks.size() - 1)
            return banks; // alone in the last bank: nowhere further
        const QString rig = bank.takeAt(at);
        if (b + 1 == banks.size())
            banks.append(QStringList());
        QStringList &next = banks[b + 1];
        next.prepend(rig);
        if (next.size() > kMaxRigs)
            bank.append(next.takeAt(1)); // the one it displaced takes its place
    } else {
        if (b == 0)
            return banks;
        const QString rig = bank.takeAt(at);
        QStringList &previous = banks[b - 1];
        previous.append(rig);
        if (previous.size() > kMaxRigs)
            bank.prepend(previous.takeAt(previous.size() - 2));
    }
    // A bank left empty goes; then the rig's row is wherever it is now.
    QList<QStringList> kept;
    for (const QStringList &one : std::as_const(banks))
        if (!one.isEmpty())
            kept.append(one);
    return kept;
}

void BanksModel::move(int steps)
{
    if (m_lifted < 0 || steps == 0)
        return;
    const auto [bank, at] = place(m_lifted);
    if (bank < 0)
        return;
    const QString name = m_banks.at(bank).at(at);
    QList<QStringList> banks = m_banks;
    int row = m_lifted;
    for (int i = 0; i < std::abs(steps); ++i)
        banks = step(banks, row, steps > 0 ? 1 : -1);
    // Where it is now, by name: a step that moved a displaced rig too has shifted rows.
    int found = 0, r = 0;
    for (const QStringList &one : std::as_const(banks))
        for (const QString &rig : one) {
            if (rig == name)
                found = r;
            ++r;
        }
    beginResetModel();
    m_banks = banks;
    m_lifted = found;
    endResetModel();
    m_ring.setHighlight(found);
    emit liftedChanged();
}

void BanksModel::moveTo(int from, int to)
{
    if (from < 0 || from >= rowCount() || to < 0 || to >= rowCount() || from == to)
        return;
    lift(from);
    move(to - from);
    drop();
}

void BanksModel::save()
{
    QJsonArray banks;
    for (const QStringList &bank : std::as_const(m_banks))
        banks.append(QJsonArray::fromStringList(bank));
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("banks"), banks}}).toJson(QJsonDocument::Compact);
    m_pedald->put(QStringLiteral("/bank"), body, [this](const PedaldReply &reply) {
        if (!reply.ok) {
            setProblem(tr("The banks weren't saved: %1").arg(reply.error));
            reload(); // back to the pedal's
            return;
        }
        setProblem({});
    });
}

void BanksModel::setProblem(const QString &problem)
{
    if (problem == m_problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

int BanksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    int count = 0;
    for (const QStringList &bank : m_banks)
        count += int(bank.size());
    return count;
}

QVariant BanksModel::data(const QModelIndex &index, int role) const
{
    const auto [bank, at] = place(index.row());
    if (!index.isValid() || bank < 0)
        return {};
    const QString name = m_banks.at(bank).at(at);
    switch (role) {
    case NameRole: return name;
    case BankRole: return tr("Bank %1").arg(bank + 1);
    case LiftedRole: return index.row() == m_lifted;
    case RunningRole: return m_rigs && m_rigs->running() == name;
    }
    return {};
}

QHash<int, QByteArray> BanksModel::roleNames() const
{
    return {{NameRole, "name"}, {BankRole, "bank"}, {LiftedRole, "lifted"}, {RunningRole, "running"}};
}

} // namespace rigpa
