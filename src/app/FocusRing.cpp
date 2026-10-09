#include "FocusRing.h"

#include <algorithm>

namespace rigpa {

FocusRing::FocusRing(QObject *parent) : QObject(parent) {}

void FocusRing::setCount(int count)
{
    count = std::max(0, count);
    if (count == m_count)
        return;
    m_count = count;
    emit countChanged();
    setHighlight(m_highlight); // clamp
    if (m_count == 0)
        setEditing(false);
}

void FocusRing::setHighlight(int index)
{
    index = m_count == 0 ? 0 : std::clamp(index, 0, m_count - 1);
    if (index == m_highlight)
        return;
    setEditing(false);
    m_highlight = index;
    emit highlightChanged();
}

void FocusRing::setEditing(bool editing)
{
    if (editing && (m_count == 0 || highlightedKind() != Kind::Value))
        editing = false;
    if (editing == m_editing)
        return;
    m_editing = editing;
    emit editingChanged();
}

void FocusRing::setKinds(const QStringList &kinds)
{
    if (kinds == m_kinds)
        return;
    m_kinds = kinds;
    emit kindsChanged();
    if (m_editing && highlightedKind() != Kind::Value)
        setEditing(false);
}

FocusRing::Kind FocusRing::kindAt(int index) const
{
    if (index < 0 || index >= m_kinds.size())
        return Kind::Item;
    const QString &kind = m_kinds.at(index);
    if (kind == QLatin1String("value"))
        return Kind::Value;
    if (kind == QLatin1String("toggle"))
        return Kind::Toggle;
    if (kind == QLatin1String("action"))
        return Kind::Action;
    return Kind::Item;
}

void FocusRing::moveBy(int steps)
{
    if (m_count > 0)
        setHighlight(m_highlight + steps);
}

} // namespace rigpa
