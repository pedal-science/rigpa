#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace rigpa {

class Navigator;
class RigEditorModel;

// --demo-animate: the screen kept busy for as long as it runs, for the D14 soak (docs/plan.md, V2),
// which needs rigpa at its most expensive beside the render core. A node is lifted and swept through
// the editor (the other tiles step aside and re-wrap, the ghost follows the pointer, the view
// auto-scrolls near the edges), then put back where it was; then the selection moves on. Nothing is
// ever dropped, so the rig the engine plays is never changed.
class DemoAnimator : public QObject
{
    Q_OBJECT
public:
    static constexpr int kFrameMs = 16;      // a pointer move per frame
    static constexpr int kSweepMs = 3000;    // one lifted node's trip
    DemoAnimator(RigEditorModel *editor, Navigator *nav, QObject *parent = nullptr);
    void start();
    int sweeps() const { return m_sweeps; }

private:
    void frame();

    RigEditorModel *m_editor;
    Navigator *m_nav;
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_from = 0;
    int m_sweeps = 0;
};

} // namespace rigpa
