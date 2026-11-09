#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class EngineSession;
class PedaldEvents;
class RigEditorModel;
class RigSession;

// The compact layout's play view: the running rig's blocks as chips, in signal order, each one a
// switch, and a line saying what changed last.
//
// The chips are the rig editor's blocks (sections are skipped, the blocks in their lanes are not),
// so a chip has the tile's name and look and the two never disagree. A tap toggles the block's
// bypass, as a double tap on its tile does.
//
// "What changed last" says who did it: rigpa's own toggles, a footswitch or MIDI with what it did
// (pedald's `action` events: "Footswitch 2 → Clean Lead"), and a value another app moved (the
// engine's events: "ljoss: Amp · Bass 6.2").
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
    void setEvents(PedaldEvents *events);
    // What a pedald `action` or an engine `rig.params` event says, as the line shows it ("" for
    // nothing worth saying). Tests call these directly.
    QString describeAction(const QJsonObject &action) const;
    QString describeParams(const QJsonObject &frame) const;

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
