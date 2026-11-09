#include "RigManager.h"

#include "RigEdits.h"
#include "pedald/PedaldEvents.h"
#include "RigList.h"
#include "RigSession.h"
#include "engine/EngineSession.h"
#include "pedald/PedaldClient.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace rigpa {

RigManager::RigManager(EngineSession *engine, RigSession *session, RigList *rigs, PedaldClient *pedald,
                       QObject *parent)
    : QObject(parent), m_engine(engine), m_session(session), m_rigs(rigs), m_pedald(pedald)
{
    connect(m_session, &RigSession::edited, this, [this] {
        if (m_rigs->running().isEmpty()) {
            setSaveState(QStringLiteral("unsaved"));
            return;
        }
        m_pending = true;
        setSaveState(QStringLiteral("pending"));
    });
    connect(m_rigs, &RigList::runningChanged, this, [this] {
        m_pending = false; // another rig: its last edit was saved as it went
        updateSaveState();
    });
    connect(m_session, &RigSession::documentChanged, this, &RigManager::updateSaveState);

    connect(&m_ring, &FocusRing::activated, this, [this](int index) {
        if (index == 0)
            confirm();
        else
            cancel();
    });
}

void RigManager::setEvents(PedaldEvents *events)
{
    connect(events, &PedaldEvents::event, this, [this](const QString &kind, const QJsonObject &data) {
        if (kind == QLatin1String("saved") && data.value(QStringLiteral("rig")).toString() == m_rigs->running()) {
            m_pending = false;
            updateSaveState();
        }
    });
}

void RigManager::setSaveState(const QString &state)
{
    if (state == m_saveState)
        return;
    m_saveState = state;
    emit saveStateChanged();
}

void RigManager::updateSaveState()
{
    if (!m_session->hasDocument())
        setSaveState({});
    else if (m_rigs->running().isEmpty())
        setSaveState(QStringLiteral("unsaved"));
    else
        setSaveState(m_pending ? QStringLiteral("pending") : QStringLiteral("saved"));
}

QJsonObject RigManager::emptyRig(const QString &name)
{
    const QJsonArray nodes{QJsonObject{{"id", "in"}, {"type", "nc.input"}},
                           QJsonObject{{"id", "out"}, {"type", "nc.output"}}};
    const QJsonArray connections{QJsonObject{{"from", "in"}, {"from_port", 0}, {"to", "out"}, {"to_port", 0}}};
    const QJsonObject chain{{"id", edits::newElementId()}, {"name", tr("Guitar")}, {"input_channel", 0},
                            {"output_channel", 0}, {"nodes", nodes}, {"connections", connections}};
    return {{"name", name}, {"chains", QJsonArray{chain}}};
}

QString RigManager::check(const QString &raw) const
{
    const QString name = raw.trimmed();
    if (name.isEmpty())
        return tr("A rig needs a name");
    if (name.size() > kMaxName)
        return tr("A rig's name is at most %1 characters").arg(kMaxName);
    if (name == QLatin1String("last") || name == QLatin1String("last.prev"))
        return tr("'%1' is kept for resuming at power-up").arg(name);
    if (name.startsWith(u'.'))
        return tr("A rig's name can't start with '.'");
    if (name.contains(u'/') || name.contains(u'\\'))
        return tr("A rig's name can't contain '/' or '\\'");
    for (const QChar c : name)
        if (c.unicode() < 0x20 || c.unicode() == 0x7f)
            return tr("A rig's name can't contain control characters");
    // A rename may change only the case of its own name.
    const bool own = m_dialog == QLatin1String("rename") && name.compare(m_target, Qt::CaseInsensitive) == 0;
    if (!own)
        for (const QString &existing : m_rigs->names())
            if (existing.compare(name, Qt::CaseInsensitive) == 0)
                return tr("There is already a rig called %1").arg(existing);
    return {};
}

void RigManager::begin(const QString &action)
{
    if (!m_dialog.isEmpty() || m_busy)
        return;
    const QString running = m_rigs->running();
    const auto unique = [this](const QString &base) {
        QString name = base;
        for (int n = 2; !check(name).isEmpty() && n < 100; ++n)
            name = base + u' ' + QString::number(n);
        return name;
    };
    m_target.clear();
    if (action == QLatin1String("new")) {
        m_dialog = action;
        m_proposed = unique(tr("New rig"));
    } else if (action == QLatin1String("saveAs")) {
        if (!m_session->hasDocument())
            return;
        m_dialog = action;
        m_target = m_session->document().name;
        m_proposed = unique(m_target.isEmpty() ? tr("Untitled") : m_target);
    } else if (action == QLatin1String("rename") || action == QLatin1String("duplicate")
               || action == QLatin1String("delete")) {
        if (running.isEmpty())
            return;
        m_dialog = action;
        m_target = running;
        m_proposed = action == QLatin1String("duplicate") ? unique(running + u' ' + tr("copy")) : running;
    } else {
        return;
    }
    m_error.clear();
    m_ring.setKinds({QStringLiteral("action"), QStringLiteral("action")});
    m_ring.setCount(2);
    m_ring.setHighlight(0);
    emit dialogErrorChanged();
    emit dialogChanged();
    emit dialogOpened();
}

void RigManager::cancel()
{
    if (m_dialog.isEmpty() || m_busy)
        return;
    finish();
}

void RigManager::setBusy(bool busy)
{
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void RigManager::finish(const QString &error)
{
    setBusy(false);
    if (!error.isEmpty()) {
        m_error = error;
        emit dialogErrorChanged();
        return;
    }
    m_dialog.clear();
    m_error.clear();
    emit dialogErrorChanged();
    emit dialogChanged();
    emit dialogClosed();
}

void RigManager::rename(const QString &from, const QString &to)
{
    // pedald renames the file, the bank's entries, the history and, when it is playing, the
    // engine's document too.
    const QByteArray body = QJsonDocument(QJsonObject{{"to", to}}).toJson(QJsonDocument::Compact);
    m_pedald->post(PedaldClient::rigPath(from, QStringLiteral("/rename")), body,
                   [this, from, to](const PedaldReply &reply) {
        if (!reply.ok) {
            finish(reply.error);
            return;
        }
        if (m_rigs->running() == from)
            m_rigs->setRunning(to);
        m_rigs->refresh();
        finish();
    });
}

void RigManager::create(const QString &name, const QJsonValue &from, bool load)
{
    const QJsonObject request{{"name", name}, {"from", from}, {"load", load}};
    m_pedald->post(QStringLiteral("/library/rigs"), QJsonDocument(request).toJson(QJsonDocument::Compact),
                   [this, name, load](const PedaldReply &reply) {
        if (!reply.ok) {
            finish(reply.error);
            return;
        }
        if (load) {
            m_rigs->setRunning(name);
            m_pending = false;
            m_session->refresh();
        }
        m_rigs->refresh();
        finish();
    });
}

void RigManager::confirm(const QString &typed)
{
    if (m_dialog.isEmpty() || m_busy)
        return;
    const QString name = (typed.isEmpty() ? m_proposed : typed).trimmed();
    const QString target = m_target;
    if (m_dialog == QLatin1String("delete")) {
        setBusy(true);
        m_pedald->remove(PedaldClient::rigPath(target), [this, target](const PedaldReply &reply) {
            if (!reply.ok) {
                finish(reply.error);
                return;
            }
            // pedald moves the pedal to the bank's next rig, as a press would.
            m_rigs->refresh();
            finish();
        });
        return;
    }
    if (m_dialog == QLatin1String("rename") && name == target) {
        finish();
        return;
    }
    const QString problem = check(name);
    if (!problem.isEmpty()) {
        m_error = problem;
        emit dialogErrorChanged();
        return;
    }
    setBusy(true);
    if (m_dialog == QLatin1String("new"))
        create(name, QJsonValue::Null, true);
    else if (m_dialog == QLatin1String("rename"))
        rename(target, name);
    else if (m_dialog == QLatin1String("duplicate"))
        // The rig as it is stored now, edits and all, without what a footswitch is doing to it.
        create(name, QStringLiteral(":running"), false);
    else if (m_dialog == QLatin1String("saveAs"))
        create(name, QStringLiteral(":running"), true);
}

} // namespace rigpa
