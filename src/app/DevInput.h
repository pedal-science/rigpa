#pragma once

#include <QObject>

class QWheelEvent;
class QKeyEvent;

namespace rigpa {

// The Mac stand-ins for the pedal's hardware controls, as an application-wide event filter:
//   ⌘ + trackpad scroll  → the main rotary (Qt reports ⌘ as ControlModifier on macOS)
//   '.'                  → apply
//   ','                  → back
// Plain scrolling is left alone, so it stays the stand-in for touch scrolling. The keys are left
// alone while a text field has focus, so a rig name can contain them.
class DevInput : public QObject
{
    Q_OBJECT

public:
    // Trackpad pixels per rotary detent, and the wheel's angle units per detent (one notch).
    static constexpr int kPixelsPerStep = 30;
    static constexpr int kAnglePerStep = 120;

    explicit DevInput(QObject *parent = nullptr);

    // When true, the rotary turns the other way for the same gesture.
    void setInverted(bool inverted) { m_inverted = inverted; }

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void rotated(int steps);
    void applied();
    void backed();

private:
    bool wheel(QWheelEvent *event);
    bool key(QKeyEvent *event);
    static bool textHasFocus();

    int m_pixels = 0;
    int m_angle = 0;
    bool m_inverted = false;
};

} // namespace rigpa
