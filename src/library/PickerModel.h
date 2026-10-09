#pragma once

#include "Library.h"
#include "app/FocusRing.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// The model/IR picker: the library's models or IRs as a list the rotary moves through, opened on
// the one a control holds now. Apply or a tap chooses; back closes without choosing. It reads the
// library rather than being it, so browsing and picking never move each other.
class PickerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the picker")
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(int kind READ kind NOTIFY openChanged)
    Q_PROPERTY(int current READ current NOTIFY openChanged)
    Q_PROPERTY(rigpa::FocusRing *ring READ ring CONSTANT)

public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, GroupRole, DetailRole, IsCurrentRole };

    explicit PickerModel(Library *library, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    int kind() const { return m_kind; }
    int current() const { return m_current; }
    FocusRing *ring() { return &m_ring; }

    // Opens on `kind` (LibraryEntry::Kind) with `currentId` highlighted; `token` comes back with
    // the choice (the control it is for).
    void open(int kind, const QString &currentId, int token);
    Q_INVOKABLE void close();
    Q_INVOKABLE void choose(int row);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void openChanged();
    void chosen(int token, const QString &id);

private:
    const QList<LibraryEntry> &entries() const { return m_library->entries(LibraryEntry::Kind(m_kind)); }

    Library *m_library;
    FocusRing m_ring;
    bool m_open = false;
    int m_kind = LibraryEntry::Model;
    int m_current = -1;
    int m_token = -1;
};

} // namespace rigpa
