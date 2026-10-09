#include "Navigator.h"

namespace rigpa {

Navigator::Navigator(QObject *parent) : QObject(parent)
{
    for (FocusRing *ring : {&m_rigList, &m_rigEditor, &m_browse, &m_settings})
        watch(ring);
}

Navigator::Overlay Navigator::overlay() const
{
    return m_overlays.isEmpty() ? NoOverlay : m_overlays.last().overlay;
}

FocusRing *Navigator::activeRing()
{
    if (!m_overlays.isEmpty())
        return m_overlays.last().ring;
    switch (m_mainView) {
    case RigView:
        return m_rigFocus == RigListFocus ? &m_rigList : &m_rigEditor;
    case BrowseView:
        return &m_browse;
    case SettingsView:
        return &m_settings;
    }
    return nullptr;
}

void Navigator::rotate(int steps)
{
    FocusRing *ring = activeRing();
    if (!ring || steps == 0)
        return;
    if (ring->editing())
        emit ring->stepped(ring->highlight(), steps);
    else
        ring->moveBy(steps);
}

void Navigator::apply()
{
    if (m_overlays.isEmpty() && m_mainView == RigView) {
        if (m_rigFocus == RigEditorFocus) {
            if (!m_rigEditor.isEmpty())
                emit nodeEditorRequested(m_rigEditor.highlight());
        } else {
            const int index = m_rigList.highlight();
            const bool any = !m_rigList.isEmpty();
            setRigFocus(RigEditorFocus);
            if (any)
                emit rigLoadRequested(index);
        }
        return;
    }
    applyInRing(activeRing());
}

void Navigator::applyInRing(FocusRing *ring)
{
    if (!ring || ring->isEmpty())
        return;
    if (ring->editing()) {
        ring->setEditing(false);
        return;
    }
    const int index = ring->highlight();
    switch (ring->highlightedKind()) {
    case FocusRing::Kind::Value:
        ring->setEditing(true);
        break;
    case FocusRing::Kind::Toggle:
        emit ring->toggled(index);
        break;
    case FocusRing::Kind::Action:
    case FocusRing::Kind::Item:
        emit ring->activated(index);
        break;
    }
}

void Navigator::back()
{
    FocusRing *ring = activeRing();
    if (ring && ring->editing()) {
        ring->setEditing(false);
        return;
    }
    if (!m_overlays.isEmpty()) {
        closeOverlay();
        return;
    }
    switch (m_mainView) {
    case RigView:
        if (m_rigFocus == RigEditorFocus)
            setRigFocus(RigListFocus);
        break; // back in the rig list does nothing: it is the top
    case BrowseView:
    case SettingsView:
        showMainView(RigView);
        break;
    }
}

void Navigator::showMainView(MainView view)
{
    FocusRing *before = activeRing();
    if (before)
        before->setEditing(false);
    const bool overlaysOpen = !m_overlays.isEmpty();
    m_overlays.clear();
    if (overlaysOpen)
        emit overlayChanged();
    if (view != m_mainView) {
        m_mainView = view;
        emit mainViewChanged();
    }
    // Arriving at rig view always lands in the editor, its default context.
    if (view == RigView && m_rigFocus != RigEditorFocus) {
        m_rigFocus = RigEditorFocus;
        emit rigFocusChanged();
    }
    changed();
}

void Navigator::focusRigList()
{
    if (m_mainView == RigView && m_overlays.isEmpty())
        setRigFocus(RigListFocus);
}

void Navigator::focusRigEditor()
{
    if (m_mainView == RigView && m_overlays.isEmpty())
        setRigFocus(RigEditorFocus);
}

void Navigator::openOverlay(Overlay overlay, FocusRing *ring)
{
    if (overlay == NoOverlay || !ring)
        return;
    if (FocusRing *below = activeRing())
        below->setEditing(false);
    watch(ring);
    m_overlays.append({overlay, ring});
    emit overlayChanged();
    changed();
}

void Navigator::closeOverlay()
{
    if (m_overlays.isEmpty())
        return;
    if (FocusRing *ring = m_overlays.last().ring)
        ring->setEditing(false);
    m_overlays.removeLast();
    emit overlayChanged();
    changed();
}

bool Navigator::hasOverlay(Overlay overlay) const
{
    for (const Layer &layer : m_overlays)
        if (layer.overlay == overlay)
            return true;
    return false;
}

void Navigator::closeOverlaysFrom(Overlay overlay)
{
    while (hasOverlay(overlay))
        closeOverlay();
}

void Navigator::setRigFocus(RigFocus focus)
{
    if (focus == m_rigFocus)
        return;
    activeRing()->setEditing(false);
    m_rigFocus = focus;
    emit rigFocusChanged();
    changed();
}

void Navigator::watch(FocusRing *ring)
{
    // Hints depend on what is highlighted and whether it is being edited.
    connect(ring, &FocusRing::highlightChanged, this, &Navigator::hintsChanged, Qt::UniqueConnection);
    connect(ring, &FocusRing::editingChanged, this, &Navigator::hintsChanged, Qt::UniqueConnection);
    connect(ring, &FocusRing::kindsChanged, this, &Navigator::hintsChanged, Qt::UniqueConnection);
    connect(ring, &FocusRing::countChanged, this, &Navigator::hintsChanged, Qt::UniqueConnection);
}

void Navigator::changed()
{
    emit activeRingChanged();
    emit hintsChanged();
}

QString Navigator::applyHint()
{
    FocusRing *ring = activeRing();
    if (!ring || ring->isEmpty())
        return {};
    if (m_overlays.isEmpty() && m_mainView == RigView)
        return m_rigFocus == RigEditorFocus ? tr("edit node") : tr("load rig");
    if (ring->editing())
        return tr("done");
    switch (ring->highlightedKind()) {
    case FocusRing::Kind::Value:
        return tr("adjust");
    case FocusRing::Kind::Toggle:
        return tr("toggle");
    case FocusRing::Kind::Action:
        return tr("press");
    case FocusRing::Kind::Item:
        return overlay() == NoOverlay ? QString() : tr("choose");
    }
    return {};
}

QString Navigator::backHint()
{
    FocusRing *ring = activeRing();
    if (ring && ring->editing())
        return tr("done");
    if (!m_overlays.isEmpty())
        return tr("close");
    switch (m_mainView) {
    case RigView:
        return m_rigFocus == RigEditorFocus ? tr("rig list") : QString();
    case BrowseView:
    case SettingsView:
        return tr("rig view");
    }
    return {};
}

} // namespace rigpa
