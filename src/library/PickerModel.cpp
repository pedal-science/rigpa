#include "PickerModel.h"

namespace rigpa {

PickerModel::PickerModel(Library *library, QObject *parent) : QAbstractListModel(parent), m_library(library)
{
    connect(&m_ring, &FocusRing::activated, this, &PickerModel::choose);
    connect(m_library, &Library::scanned, this, [this] {
        if (!m_open)
            return;
        beginResetModel();
        endResetModel();
        m_ring.setCount(int(entries().size()));
    });
}

void PickerModel::open(int kind, const QString &currentId, int token)
{
    beginResetModel();
    m_kind = kind;
    m_token = token;
    m_current = -1;
    const auto &list = entries();
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i).id == currentId)
            m_current = i;
    m_open = true;
    endResetModel();
    m_ring.setKinds({});
    m_ring.setCount(int(list.size()));
    m_ring.setHighlight(std::max(0, m_current));
    emit openChanged();
}

void PickerModel::close()
{
    if (!m_open)
        return;
    m_open = false;
    m_ring.setEditing(false);
    emit openChanged();
}

void PickerModel::choose(int row)
{
    if (!m_open || row < 0 || row >= entries().size())
        return;
    const QString id = entries().at(row).id;
    const int token = m_token;
    close();
    emit chosen(token, id);
}

int PickerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(entries().size());
}

QVariant PickerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= entries().size())
        return {};
    const LibraryEntry &e = entries().at(index.row());
    switch (role) {
    case IdRole:
        return e.id;
    case NameRole:
        return e.name;
    case GroupRole:
        return e.group;
    case DetailRole:
        return Library::detail(e);
    case IsCurrentRole:
        return index.row() == m_current;
    }
    return {};
}

QHash<int, QByteArray> PickerModel::roleNames() const
{
    return {{IdRole, "entryId"}, {NameRole, "name"}, {GroupRole, "group"}, {DetailRole, "detail"},
            {IsCurrentRole, "isCurrent"}};
}

} // namespace rigpa
