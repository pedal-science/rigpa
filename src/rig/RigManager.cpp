#include "RigManager.h"

#include "RigEdits.h"
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
    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, &QTimer::timeout, this, &RigManager::saveNow);
    connect(m_session, &RigSession::edited, this, [this] {
        m_saveTo = m_rigs->running();
        if (m_saveTo.isEmpty()) {
            setSaveState(QStringLiteral("unsaved"));
            return;
        }
        setSaveState(QStringLiteral("pending"));
        m_saveTimer.start(kSaveDelayMs);
    });
    connect(m_rigs, &RigList::runningChanged, this, &RigManager::updateSaveState);
    connect(m_session, &RigSession::documentChanged, this, &RigManager::updateSaveState);

    connect(&m_ring, &FocusRing::activated, this, [this](int index) {
        if (index == 0)
            confirm();
        else
            cancel();
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
    if (!m_session->hasDocument()) {
        setSaveState({});
        return;
    }
    if (m_saving || m_saveTimer.isActive())
        return;
    if (m_rigs->running().isEmpty())
        setSaveState(QStringLiteral("unsaved"));
    else if (m_saveState != QLatin1String("failed"))
        setSaveState(QStringLiteral("saved"));
}

void RigManager::saveNow()
{
    m_saveTimer.stop();
    if (m_saveTo.isEmpty() || m_saving)
        return;
    // Edits still on their way: the engine's document isn't the last one yet.
    if (m_session->applying() || m_session->edits()->busy()) {
        m_saveTimer.start(200);
        return;
    }
    m_saving = true;
    setSaveState(QStringLiteral("saving"));
    const QString name = m_saveTo;
    // Whatever was waiting for this save goes next, whether it worked or not.
    const auto done = [this] {
        m_saving = false;
        for (auto &then : std::exchange(m_afterSave, {}))
            then();
    };
    m_engine->client()->request(QStringLiteral("rig.get"), {}, [this, name, done](const EngineReply &reply) {
        if (!reply.ok) {
            done();
            setSaveState(QStringLiteral("failed"));
            m_session->say(tr("Couldn't save %1: %2").arg(name, reply.message.isEmpty() ? reply.code : reply.message));
            return;
        }
        put(name, reply.body.value(QStringLiteral("document")).toObject(), [this, name, done](const QString &error) {
            if (!error.isEmpty()) {
                setSaveState(QStringLiteral("failed"));
                m_session->say(tr("Couldn't save %1: %2").arg(name, error));
            } else {
                // Another edit came while this one was saving: its own save is waiting.
                setSaveState(m_saveTimer.isActive() ? QStringLiteral("pending") : QStringLiteral("saved"));
            }
            done();
        });
    });
}

void RigManager::whenSaved(std::function<void()> then)
{
    if (!m_saving && !m_saveTimer.isActive()) {
        then();
        return;
    }
    m_afterSave.append(std::move(then));
    if (m_saveTimer.isActive())
        saveNow();
}

void RigManager::put(const QString &name, QJsonObject document, std::function<void(const QString &)> done)
{
    document.insert(QStringLiteral("name"), name); // pedald: a rig's name is its file's
    m_pedald->put(PedaldClient::rigPath(name), QJsonDocument(document).toJson(QJsonDocument::Compact),
                  [done](const PedaldReply &reply) { done(reply.ok ? QString() : reply.error); });
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
    const QByteArray body = QJsonDocument(QJsonObject{{"to", to}}).toJson(QJsonDocument::Compact);
    m_pedald->post(PedaldClient::rigPath(from, QStringLiteral("/rename")), body,
                   [this, from, to](const PedaldReply &reply) {
        if (!reply.ok) {
            finish(reply.error);
            return;
        }
        if (m_rigs->running() == from) {
            m_session->renameRunning(to);
            m_rigs->setRunning(to);
        }
        m_rigs->refresh();
        finish();
    });
}

void RigManager::duplicate(const QString &from, const QString &to)
{
    m_pedald->get(PedaldClient::rigPath(from), [this, to](const PedaldReply &reply) {
        if (!reply.ok) {
            finish(reply.error);
            return;
        }
        put(to, reply.body.object(), [this](const QString &error) {
            if (error.isEmpty())
                m_rigs->refresh();
            finish(error);
        });
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
            if (m_rigs->running() == target)
                m_rigs->setRunning({}); // still playing, but no longer in the library
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
    if (m_dialog == QLatin1String("new")) {
        put(name, emptyRig(name), [this, name](const QString &error) {
            if (!error.isEmpty()) {
                finish(error);
                return;
            }
            m_rigs->refresh();
            m_session->loadRigNamed(name);
            finish();
        });
    } else if (m_dialog == QLatin1String("rename")) {
        // Anything waiting to be saved goes to the old name first, which pedald then renames.
        whenSaved([this, target, name] { rename(target, name); });
    } else if (m_dialog == QLatin1String("duplicate")) {
        // A copy of the rig as it is now, saved first.
        whenSaved([this, target, name] { duplicate(target, name); });
    } else if (m_dialog == QLatin1String("saveAs")) {
        put(name, m_session->document().json, [this, name](const QString &error) {
            if (!error.isEmpty()) {
                finish(error);
                return;
            }
            m_session->renameRunning(name);
            m_rigs->setRunning(name);
            m_rigs->refresh();
            setSaveState(QStringLiteral("saved"));
            finish();
        });
    }
}

} // namespace rigpa
