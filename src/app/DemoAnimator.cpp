#include "DemoAnimator.h"

#include "Navigator.h"
#include "rig/RigEditorModel.h"

#include <cmath>

namespace rigpa {

DemoAnimator::DemoAnimator(RigEditorModel *editor, Navigator *nav, QObject *parent)
    : QObject(parent), m_editor(editor), m_nav(nav)
{
    m_timer.setInterval(kFrameMs);
    connect(&m_timer, &QTimer::timeout, this, &DemoAnimator::frame);
}

void DemoAnimator::start()
{
    m_clock.start();
    m_timer.start();
}

void DemoAnimator::frame()
{
    const int count = m_editor->selectableCount();
    if (count < 2)
        return; // nothing to move past: wait for a rig with two nodes or more
    if (!m_editor->dragging()) {
        m_from = (m_from + 1) % count;
        if (!m_editor->lift(m_editor->elementAt(m_from)))
            return;
        m_clock.restart();
    }
    const double t = double(m_clock.elapsed()) / kSweepMs;
    if (t >= 1) {
        // Back where it was, and the selection on to the next node: the rig is never changed.
        m_editor->cancelDrag();
        m_nav->rotate(1);
        ++m_sweeps;
        return;
    }
    // Out along the selectable tiles and back, through each one's left edge (where a drop would
    // land), eased between them, so the layout reflows at every step.
    const double along = (t < 0.5 ? t * 2 : (1 - t) * 2) * (count - 1);
    const int a = int(std::floor(along)), b = std::min(a + 1, count - 1);
    const double f = along - a;
    const QRectF ra = m_editor->rectOf(a), rb = m_editor->rectOf(b);
    m_editor->dragTo(QPointF(ra.left() + 12 + (rb.left() - ra.left()) * f,
                             ra.center().y() + (rb.center().y() - ra.center().y()) * f));
}

} // namespace rigpa
