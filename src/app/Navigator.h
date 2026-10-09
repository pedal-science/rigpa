#pragma once

#include "FocusRing.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// Where the user is, and what the main rotary, apply and back do there: the table in
// docs/spec.md ("Hardware control model") is this class's specification and its test oracle.
//
// Touch never goes through here except to move the highlight (any touch moves the hardware
// highlight to what was touched), so the two input paths never disagree.
class Navigator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the navigator")
    Q_PROPERTY(MainView mainView READ mainView NOTIFY mainViewChanged)
    Q_PROPERTY(RigFocus rigFocus READ rigFocus NOTIFY rigFocusChanged)
    Q_PROPERTY(Overlay overlay READ overlay NOTIFY overlayChanged)
    Q_PROPERTY(rigpa::FocusRing *rigList READ rigList CONSTANT)
    Q_PROPERTY(rigpa::FocusRing *rigEditor READ rigEditor CONSTANT)
    Q_PROPERTY(rigpa::FocusRing *browse READ browse CONSTANT)
    Q_PROPERTY(rigpa::FocusRing *settings READ settings CONSTANT)
    Q_PROPERTY(rigpa::FocusRing *activeRing READ activeRing NOTIFY activeRingChanged)
    Q_PROPERTY(QString applyHint READ applyHint NOTIFY hintsChanged)
    Q_PROPERTY(QString backHint READ backHint NOTIFY hintsChanged)

public:
    enum MainView { RigView, BrowseView, SettingsView };
    Q_ENUM(MainView)
    enum RigFocus { RigListFocus, RigEditorFocus };
    Q_ENUM(RigFocus)
    // Things opened on top of a main view, deepest last.
    enum Overlay { NoOverlay, NodeEditor, Picker, Palette, Dialog };
    Q_ENUM(Overlay)

    explicit Navigator(QObject *parent = nullptr);

    MainView mainView() const { return m_mainView; }
    RigFocus rigFocus() const { return m_rigFocus; }
    Overlay overlay() const;
    FocusRing *rigList() { return &m_rigList; }
    FocusRing *rigEditor() { return &m_rigEditor; }
    FocusRing *browse() { return &m_browse; }
    FocusRing *settings() { return &m_settings; }
    FocusRing *activeRing();
    QString applyHint();
    QString backHint();

    // The three hardware controls (on the Mac: ⌘ + scroll, '.' and ',').
    Q_INVOKABLE void rotate(int steps);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void back();

    // Menu buttons and touch.
    Q_INVOKABLE void showMainView(rigpa::Navigator::MainView view);
    Q_INVOKABLE void focusRigList();
    Q_INVOKABLE void focusRigEditor();
    // An overlay brings its own ring (the node editor's controls, a picker's list).
    Q_INVOKABLE void openOverlay(rigpa::Navigator::Overlay overlay, rigpa::FocusRing *ring);
    Q_INVOKABLE void closeOverlay();

signals:
    void mainViewChanged();
    void rigFocusChanged();
    void overlayChanged();
    void activeRingChanged();
    void hintsChanged();
    // Apply in the rig editor: open the editor for this node.
    void nodeEditorRequested(int index);
    // Apply in the rig list: load this rig (focus has already moved to its editor).
    void rigLoadRequested(int index);

private:
    struct Layer {
        Overlay overlay;
        QPointer<FocusRing> ring;
    };

    void setRigFocus(RigFocus focus);
    void applyInRing(FocusRing *ring);
    void watch(FocusRing *ring);
    void changed();

    MainView m_mainView = RigView;
    RigFocus m_rigFocus = RigEditorFocus;
    QList<Layer> m_overlays;
    FocusRing m_rigList;
    FocusRing m_rigEditor;
    FocusRing m_browse;
    FocusRing m_settings;
};

} // namespace rigpa
