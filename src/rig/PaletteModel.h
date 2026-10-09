#pragma once

#include "RigDocument.h"
#include "app/FocusRing.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace rigpa {

class EngineSession;
class Library;

// What can be added to a rig (the header's +): an amp (a NAM model and a tone stack, as ljoss
// makes it), a parallel split, and every node type the engine's schema offers that a rig is
// built from (not its plumbing: input, output, split, mix). The rotary moves through the list,
// apply or a tap chooses, back closes.
class PaletteModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the palette")
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(rigpa::FocusRing *ring READ ring CONSTANT)

public:
    enum Role { NameRole = Qt::UserRole + 1, DetailRole, TintRole };

    PaletteModel(EngineSession *engine, Library *library, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    FocusRing *ring() { return &m_ring; }
    void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void choose(int row);

    // The element a row makes, with new ids; a model or IR it needs is the library's first, to be
    // changed in its editor. Nothing when it can't be made (an empty library), with `why`.
    std::optional<RigElement> make(int row, QString *why = nullptr) const;
    int indexOfType(const QString &type) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void openChanged();
    void chosen(int row);

private:
    struct Entry {
        QString kind;   // "amp", "section", "type"
        QString type;   // for "type"
        QString name, detail;
    };
    void rebuild();
    std::optional<RigNode> node(const QString &element, const QString &type, QString *why) const;

    EngineSession *m_engine;
    Library *m_library;
    FocusRing m_ring;
    QList<Entry> m_entries;
    bool m_open = false;
};

} // namespace rigpa
