#include "RigList.h"

#include <QJsonArray>
#include <QJsonObject>

namespace rigpa {

RigList::RigList(PedaldClient *pedald, QObject *parent) : QAbstractListModel(parent), m_pedald(pedald)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &RigList::refresh);
}

void RigList::start()
{
    m_timer.start();
    refresh();
}

void RigList::refresh()
{
    if (m_inFlight)
        return;
    m_inFlight = true;
    m_pedald->get(QStringLiteral("/library/rigs"), [this](const PedaldReply &reply) {
        m_inFlight = false;
        if (!reply.ok) {
            const QString problem = tr("rig library: %1").arg(reply.error);
            if (problem != m_problem) {
                m_problem = problem;
                emit changed();
            }
            return;
        }
        QStringList names;
        for (const auto &r : reply.body.object().value(QStringLiteral("rigs")).toArray())
            names.append(r.toObject().value(QStringLiteral("name")).toString());
        m_problem.clear();
        setNames(names);
    });
}

void RigList::setNames(const QStringList &names)
{
    m_loaded = true;
    if (names != m_names) {
        beginResetModel();
        m_names = names;
        endResetModel();
        emit runningChanged(); // the index moves with the list
    }
    emit changed();
}

void RigList::setRunning(const QString &name)
{
    if (name == m_running)
        return;
    const int before = runningIndex();
    m_running = name;
    const int after = runningIndex();
    for (int row : {before, after})
        if (row >= 0)
            emit dataChanged(index(row), index(row), {RunningRole});
    emit runningChanged();
}

int RigList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RigList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_names.size())
        return {};
    const QString &name = m_names.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return name;
    case RunningRole: return name == m_running;
    }
    return {};
}

QHash<int, QByteArray> RigList::roleNames() const
{
    return {{NameRole, "name"}, {RunningRole, "running"}};
}

} // namespace rigpa
