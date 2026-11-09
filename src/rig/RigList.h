#pragma once

#include "pedald/PedaldClient.h"

#include <QAbstractListModel>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// The library's rigs, by name, from pedald (GET /library/rigs): pedald owns the rig library
// (pedal.project D4). Read when pedald's events say the library changed, and on each connection.
class RigList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the rig list")
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString running READ running NOTIFY runningChanged)
    Q_PROPERTY(int runningIndex READ runningIndex NOTIFY runningChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, RunningRole };
    explicit RigList(PedaldClient *pedald, QObject *parent = nullptr);
    // pedald's events: a `library` change of rigs, or a `hello`, is a reason to read the list again.
    void setEvents(class PedaldEvents *events);

    void start();
    Q_INVOKABLE void refresh();
    void setNames(const QStringList &names); // tests, and refresh's answer

    int count() const { return int(m_names.size()); }
    bool loaded() const { return m_loaded; }
    QString problem() const { return m_problem; }
    QStringList names() const { return m_names; }
    Q_INVOKABLE QString nameAt(int index) const { return m_names.value(index); }

    // Which library rig the engine runs, or "" (set by RigSession, which knows the engine).
    QString running() const { return m_running; }
    int runningIndex() const { return int(m_names.indexOf(m_running)); }
    void setRunning(const QString &name);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void changed();
    void runningChanged();

private:
    PedaldClient *m_pedald;
    QStringList m_names;
    QString m_running;
    QString m_problem;
    bool m_loaded = false;
    bool m_inFlight = false;
    bool m_again = false;
};

} // namespace rigpa
