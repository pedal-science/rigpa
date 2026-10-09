#pragma once

#include "RigDocument.h"
#include "app/FocusRing.h"
#include "engine/Schema.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class Library;
class RigEditorModel;
class RigSession;
class EngineSession;

// The node editor: one control for every parameter of the selected block's nodes, built from
// `engine.schema` (docs/spec.md, "Node editor"). Numbers are knobs, small option sets are
// segmented choices, larger ones are stepped knobs, flags are toggles, and a model or IR is a
// picker. The list is in reading order, which is also the order of the editor's FocusRing; after
// the controls the ring has Bypass (when the block can be bypassed) and Close.
//
// Values are shown as the engine has them (the document is read again after every edit). While a
// control is being changed here its own value wins for a moment, so a knob doesn't jump back to an
// older reading. A parameter the engine ramps is streamed as it moves (EditStream); any other is
// sent once, when the drag ends or the control is deselected.
class NodeEditorModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the node editor")
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QString element READ element NOTIFY openChanged)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)
    Q_PROPERTY(bool bypassable READ bypassable NOTIFY changed)
    Q_PROPERTY(bool bypassed READ bypassed NOTIFY changed)
    Q_PROPERTY(int bypassIndex READ bypassIndex NOTIFY changed)
    Q_PROPERTY(int closeIndex READ closeIndex NOTIFY changed)
    Q_PROPERTY(QString note READ note NOTIFY changed)
    Q_PROPERTY(rigpa::FocusRing *ring READ ring CONSTANT)

public:
    // How long a control's own value outlasts readings from the engine after it last moved.
    static constexpr int kHoldMs = 1200;
    // Option sets up to this size are a segmented choice; larger ones step on a knob.
    static constexpr int kMaxSegments = 5;
    // Rotary detents across a continuous parameter's whole range.
    static constexpr int kDetents = 100;

    enum Role {
        KindRole = Qt::UserRole + 1, LabelRole, GroupRole, PositionRole, TextRole, OptionsRole,
        CurrentRole, StreamingRole, FirstOfGroupRole,
    };

    NodeEditorModel(RigSession *session, EngineSession *engine, RigEditorModel *editor, Library *library,
                    QObject *parent = nullptr);

    bool isOpen() const { return !m_element.isEmpty(); }
    QString element() const { return m_element; }
    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    bool bypassable() const { return m_bypassable; }
    bool bypassed() const;
    int bypassIndex() const { return m_bypassable ? int(m_controls.size()) : -1; }
    int closeIndex() const { return int(m_controls.size()) + (m_bypassable ? 1 : 0); }
    QString note() const { return m_note; }
    FocusRing *ring() { return &m_ring; }

    // Opens the editor on a block or section (by element), or closes it.
    Q_INVOKABLE void open(const QString &element);
    Q_INVOKABLE void close();

    // Touch: a knob dragged to `position` (0–1 along its travel), and let go.
    Q_INVOKABLE void setPosition(int index, double position);
    Q_INVOKABLE void release(int index);
    // The rotary, while a control is selected: `steps` detents.
    Q_INVOKABLE void step(int index, int steps);
    // A choice or a toggle.
    Q_INVOKABLE void choose(int index, int option);
    Q_INVOKABLE void toggleBypass();
    // The picker chose `id` for the control at `index`.
    Q_INVOKABLE void pick(int index, const QString &id);
    // What the picker should show for a control: 0 models, 1 IRs; and the id it holds now.
    Q_INVOKABLE int pickerKind(int index) const;
    Q_INVOKABLE QString pickerCurrent(int index) const;

    // For tests and the shell: a control's parameter, and its value as the editor holds it.
    QString paramAt(int index) const;
    double valueAt(int index) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // A value for people: "+1.2 dB", "9.00 kHz", "250 ms".
    static QString format(const ParamSpec &p, double value);
    // A number's place along a knob's travel (0–1), and back: logarithmic where the schema says so.
    static double positionOf(const ParamSpec &p, double value);
    static double valueAt(const ParamSpec &p, double position);

signals:
    void openChanged();
    void changed();
    // Close was pressed (the shell closes the overlay).
    void closeRequested();
    // A picker control was activated: the shell opens the picker over the editor.
    void pickerRequested(int index);

private:
    struct Control {
        QString kind;     // "knob", "choice", "picker", "toggle"
        QString group;    // the node's type name, shown above its first control
        bool firstOfGroup = false;
        RigNode node;
        ParamSpec spec;
        double value = 0;     // a number, a flag, or an option's index
        QString ref;          // a picker's library id
        bool dirty = false;   // changed here, not yet sent (a parameter the engine doesn't ramp)
        QElapsedTimer held;   // since it last moved here
    };

    void rebuild();
    void readValues(bool keepHeld);
    void readValue(Control &c, bool keepHeld) const;
    void updateRing();
    void touch(int index);
    void send(int index);
    void setValue(int index, double value);
    bool streams(const Control &c) const;
    QString textOf(const Control &c) const;

    RigSession *m_session;
    EngineSession *m_engine;
    RigEditorModel *m_editor;
    Library *m_library;
    FocusRing m_ring;
    QString m_element;
    QString m_chain;
    QString m_title, m_subtitle, m_note;
    bool m_bypassable = false;
    QList<Control> m_controls;
    QStringList m_shape;   // node ids and types: whether a re-read changed what the editor shows
    int m_lastEdited = -1;
};

} // namespace rigpa
