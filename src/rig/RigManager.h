#pragma once

#include "app/FocusRing.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class EngineSession;
class PedaldClient;
class RigList;
class RigSession;

// Keeping the library in step with what is played, through pedald (it owns the library, D4).
//
// Auto-save: about a second after the last edit made here, the document the engine runs is read
// back and PUT to the library rig it came from (the running rig, as the rig list names it). A rig
// that isn't in the library (one ljoss applied) is never saved over anything: it shows as unsaved,
// and Save as… gives it a name.
//
// Rig management: New, Rename, Duplicate and Delete act on the running rig, Save as… on a running
// rig the library doesn't have. Each asks first, in a dialog (a name, typed on the virtual
// keyboard, or "are you sure"); its ring is OK and Cancel, so apply and back answer it too.
class RigManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the rig manager")
    Q_PROPERTY(QString saveState READ saveState NOTIFY saveStateChanged)
    Q_PROPERTY(QString dialog READ dialog NOTIFY dialogChanged)
    Q_PROPERTY(QString dialogTarget READ dialogTarget NOTIFY dialogChanged)
    Q_PROPERTY(QString proposedName READ proposedName NOTIFY dialogChanged)
    Q_PROPERTY(QString dialogError READ dialogError NOTIFY dialogErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(rigpa::FocusRing *ring READ ring CONSTANT)

public:
    static constexpr int kSaveDelayMs = 1000;
    static constexpr int kMaxName = 200; // pedald's MAX_NAME

    RigManager(EngineSession *engine, RigSession *session, RigList *rigs, PedaldClient *pedald,
               QObject *parent = nullptr);

    // "" (no rig), "saved", "pending" (an edit waits for its save), "saving", "unsaved" (the
    // running rig isn't in the library), "failed".
    QString saveState() const { return m_saveState; }
    // "" (none), "new", "rename", "duplicate", "saveAs", "delete".
    QString dialog() const { return m_dialog; }
    QString dialogTarget() const { return m_target; }
    QString proposedName() const { return m_proposed; }
    QString dialogError() const { return m_error; }
    bool busy() const { return m_busy; }
    FocusRing *ring() { return &m_ring; }

    Q_INVOKABLE void begin(const QString &action);
    // What the dialog's field says now (OK from the hardware uses it).
    Q_INVOKABLE void setProposedName(const QString &name) { m_proposed = name; }
    // OK, with the name typed (ignored by delete).
    Q_INVOKABLE void confirm(const QString &name = {});
    Q_INVOKABLE void cancel();
    // Why a name can't be used ("" when it can): pedald's rules, and no clash with another rig.
    Q_INVOKABLE QString check(const QString &name) const;
    // Saves now (tests; and before anything that needs the file current).
    void saveNow();

    // The document a new rig starts with: one chain, In to Out.
    static QJsonObject emptyRig(const QString &name);

signals:
    void saveStateChanged();
    void dialogChanged();
    void dialogErrorChanged();
    void busyChanged();
    // A dialog opened or closed (the shell puts its overlay up or down).
    void dialogOpened();
    void dialogClosed();

private:
    void setSaveState(const QString &state);
    void updateSaveState();
    void finish(const QString &error = {});
    void setBusy(bool busy);
    // Runs `then` once nothing is waiting to be saved (at once, if nothing is).
    void whenSaved(std::function<void()> then);
    void rename(const QString &from, const QString &to);
    void duplicate(const QString &from, const QString &to);
    void put(const QString &name, QJsonObject document, std::function<void(const QString &error)> done);

    EngineSession *m_engine;
    RigSession *m_session;
    RigList *m_rigs;
    PedaldClient *m_pedald;
    FocusRing m_ring;
    QTimer m_saveTimer;
    QString m_saveTo;
    QString m_saveState;
    bool m_saving = false;
    QList<std::function<void()>> m_afterSave;
    QString m_dialog, m_target, m_proposed, m_error;
    bool m_busy = false;
};

} // namespace rigpa
