#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// One place the hardware controls can move through: a row of controls in an editor, or the items
// of a list. The rotary moves the highlight; apply either toggles, activates, or makes a value
// control "selected" (editing), after which the rotary changes the value instead (spec, the
// hardware control model). The view owns what the items are; the ring only knows how many there
// are and what kind each one is.
class FocusRing : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int highlight READ highlight WRITE setHighlight NOTIFY highlightChanged)
    Q_PROPERTY(bool editing READ editing WRITE setEditing NOTIFY editingChanged)
    // One entry per item: "item", "value", "toggle" or "action". Missing entries are "item".
    Q_PROPERTY(QStringList kinds READ kinds WRITE setKinds NOTIFY kindsChanged)

public:
    enum class Kind { Item, Value, Toggle, Action };

    explicit FocusRing(QObject *parent = nullptr);

    int count() const { return m_count; }
    void setCount(int count);
    int highlight() const { return m_highlight; }
    void setHighlight(int index);
    bool editing() const { return m_editing; }
    void setEditing(bool editing);
    QStringList kinds() const { return m_kinds; }
    void setKinds(const QStringList &kinds);

    Kind kindAt(int index) const;
    Kind highlightedKind() const { return kindAt(m_highlight); }
    bool isEmpty() const { return m_count == 0; }

    // Moves the highlight, stopping at either end (the rotary does not wrap).
    Q_INVOKABLE void moveBy(int steps);

signals:
    void countChanged();
    void highlightChanged();
    void editingChanged();
    void kindsChanged();
    // The rotary turned while the highlighted value control is selected.
    void stepped(int index, int delta);
    // Apply on a boolean control.
    void toggled(int index);
    // Apply on an item or an action.
    void activated(int index);

private:
    int m_count = 0;
    int m_highlight = 0;
    bool m_editing = false;
    QStringList m_kinds;
};

} // namespace rigpa
