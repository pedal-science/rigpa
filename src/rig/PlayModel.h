#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class EngineSession;
class RigEditorModel;
class RigSession;

// The compact layout's play view: the running rig's blocks as chips, in signal order, each one a
// switch, and a line saying what changed last.
//
// The chips are the rig editor's blocks (sections are skipped, the blocks in their lanes are not),
// so a chip has the tile's name and look and the two never disagree. A tap toggles the block's
// bypass, as a double tap on its tile does.
//
// "What changed last" is what rigpa can know today: its own toggles, and the rig changing under
// it. The cause of a change made elsewhere (a footswitch, ljoss) arrives with pedald's events
// (pedal.project docs/pedal-owns-state.md, O5).
class PlayModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the play model")
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString lastChange READ lastChange NOTIFY lastChangeChanged)

public:
    enum Role { ElementRole = Qt::UserRole + 1, TitleRole, SubtitleRole, ColorRole, BypassedRole, MissingRole };

    PlayModel(RigSession *session, RigEditorModel *editor, EngineSession *engine, QObject *parent = nullptr);

    int count() const { return int(m_chips.size()); }
    QString lastChange() const { return m_lastChange; }
    Q_INVOKABLE QString elementAt(int row) const;
    // Switches the chip's block on or off.
    Q_INVOKABLE void toggle(int row);
    void setLastChange(const QString &text);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void changed();
    void lastChangeChanged();

private:
    struct Chip {
        QString element, title, subtitle, color;
        bool bypassed = false, missing = false;
        bool operator==(const Chip &) const = default;
    };

    void rebuild();
    void rigChanged();

    RigSession *m_session;
    RigEditorModel *m_editor;
    EngineSession *m_engine;
    QList<Chip> m_chips;
    QString m_lastChange;
    QString m_rigName;
};

} // namespace rigpa
