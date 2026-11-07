#include "PlayModel.h"

#include "RigEditorModel.h"
#include "RigSession.h"
#include "engine/EngineSession.h"

#include <algorithm>

namespace rigpa {

PlayModel::PlayModel(RigSession *session, RigEditorModel *editor, EngineSession *engine, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_editor(editor), m_engine(engine)
{
    connect(m_editor, &RigEditorModel::laidOut, this, &PlayModel::rebuild);
    if (m_engine)
        connect(m_engine, &EngineSession::rigChanged, this, &PlayModel::rigChanged);
    rebuild();
}

void PlayModel::rebuild()
{
    QList<std::pair<int, Chip>> blocks;
    for (const RigEditorModel::Item &item : m_editor->items()) {
        // The lifted element, mid-drag, is the editor's business: the play view isn't showing then.
        if (item.kind != QLatin1String("block") || item.select < 0 || item.lift != 0)
            continue;
        Chip chip;
        chip.element = item.element;
        chip.title = item.title;
        chip.subtitle = item.subtitle;
        chip.color = item.color;
        chip.bypassed = item.bypassed;
        chip.missing = item.missing;
        blocks.append({item.select, chip});
    }
    std::stable_sort(blocks.begin(), blocks.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    QList<Chip> chips;
    for (const auto &[select, chip] : blocks)
        chips.append(chip);
    if (chips == m_chips)
        return;

    if (chips.size() == m_chips.size()) {
        // The same blocks, one of them switched: a change of data, not a new list, so a chip that
        // was just tapped (or highlighted by the rotary) stays where it is.
        m_chips = chips;
        emit dataChanged(index(0), index(int(m_chips.size()) - 1));
    } else {
        beginResetModel();
        m_chips = chips;
        endResetModel();
    }
    emit changed();
}

void PlayModel::rigChanged()
{
    const QString name = m_engine->rigName();
    if (name == m_rigName)
        return;
    const bool first = m_rigName.isEmpty();
    m_rigName = name;
    if (!first && !name.isEmpty())
        setLastChange(tr("Now playing %1").arg(name));
}

QString PlayModel::elementAt(int row) const
{
    return row >= 0 && row < m_chips.size() ? m_chips.at(row).element : QString();
}

void PlayModel::toggle(int row)
{
    if (!m_session || row < 0 || row >= m_chips.size())
        return;
    const Chip &chip = m_chips.at(row);
    const bool bypass = !m_session->isBypassed(chip.element);
    m_session->setBypass(chip.element, bypass);
    setLastChange(bypass ? tr("%1 off").arg(chip.title) : tr("%1 on").arg(chip.title));
}

void PlayModel::setLastChange(const QString &text)
{
    if (text == m_lastChange)
        return;
    m_lastChange = text;
    emit lastChangeChanged();
}

int PlayModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_chips.size());
}

QVariant PlayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_chips.size())
        return {};
    const Chip &chip = m_chips.at(index.row());
    switch (role) {
    case ElementRole: return chip.element;
    case TitleRole: return chip.title;
    case SubtitleRole: return chip.subtitle;
    case ColorRole: return chip.color;
    case BypassedRole: return chip.bypassed;
    case MissingRole: return chip.missing;
    }
    return {};
}

QHash<int, QByteArray> PlayModel::roleNames() const
{
    return {
        {ElementRole, "element"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"},
        {ColorRole, "chipColor"}, {BypassedRole, "bypassed"}, {MissingRole, "missing"},
    };
}

} // namespace rigpa
