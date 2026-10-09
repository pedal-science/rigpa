#include "DevInput.h"

#include <QGuiApplication>
#include <QInputMethodQueryEvent>
#include <QKeyEvent>
#include <QWheelEvent>

#include <cstdlib>

namespace rigpa {

DevInput::DevInput(QObject *parent) : QObject(parent) {}

bool DevInput::eventFilter(QObject *watched, QEvent *event)
{
    // Events reach the filter once per object they are delivered to; acting only on the window's
    // own delivery keeps one gesture one rotation.
    if (!watched->isWindowType())
        return false;
    switch (event->type()) {
    case QEvent::Wheel:
        return wheel(static_cast<QWheelEvent *>(event));
    case QEvent::KeyPress:
        return key(static_cast<QKeyEvent *>(event));
    default:
        return false;
    }
}

bool DevInput::wheel(QWheelEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier)) {
        m_pixels = m_angle = 0;
        return false;
    }
    // The trackpad keeps sending after the fingers lift; a rotary does not coast.
    if (event->phase() == Qt::ScrollMomentum)
        return true;

    // Whichever axis moved most; down or right (as the content moves) is the next item.
    const QPoint pixels = event->pixelDelta();
    const QPoint angle = event->angleDelta();
    auto along = [](QPoint p) { return std::abs(p.y()) >= std::abs(p.x()) ? -p.y() : -p.x(); };
    const int sign = m_inverted ? -1 : 1;

    int steps = 0;
    if (!pixels.isNull()) {
        m_pixels += sign * along(pixels);
        steps = m_pixels / kPixelsPerStep;
        m_pixels -= steps * kPixelsPerStep;
    } else {
        m_angle += sign * along(angle);
        steps = m_angle / kAnglePerStep;
        m_angle -= steps * kAnglePerStep;
    }
    if (steps != 0)
        emit rotated(steps);
    return true;
}

bool DevInput::key(QKeyEvent *event)
{
    if (event->isAutoRepeat() || textHasFocus())
        return false;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    if (event->key() == Qt::Key_Period) {
        emit applied();
        return true;
    }
    if (event->key() == Qt::Key_Comma) {
        emit backed();
        return true;
    }
    return false;
}

bool DevInput::textHasFocus()
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return false;
    QInputMethodQueryEvent query(Qt::ImEnabled);
    QCoreApplication::sendEvent(focus, &query);
    return query.value(Qt::ImEnabled).toBool();
}

} // namespace rigpa
