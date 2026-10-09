#include "RigSession.h"

#include <QFileInfo>
#include <QJsonArray>

namespace rigpa {

RigSession::RigSession(EngineSession *engine, RigList *rigs, QObject *parent)
    : QObject(parent), m_engine(engine), m_rigs(rigs)
{
    connect(m_engine, &EngineSession::rigChanged, this, [this] {
        updateRunning();
        if (m_engine->connection() == EngineSession::Connected && m_engine->hasRig())
            fetchDocument();
        else if (!m_engine->hasRig() && m_hasDocument) {
            m_hasDocument = false;
            m_document = {};
            emit documentChanged();
        }
        maybeLoadFirst();
    });
    connect(m_engine, &EngineSession::statusChanged, this, [this] {
        // A smoothed knob moves params_applied without a new generation: re-read for that too.
        if (m_engine->paramsApplied() != m_seenParams && m_engine->hasRig()) {
            m_seenParams = m_engine->paramsApplied();
            fetchDocument();
        }
        maybeLoadFirst();
    });
    connect(m_engine, &EngineSession::connectionChanged, this, [this] {
        if (m_engine->connection() != EngineSession::Connected)
            m_startupDone = false; // the rule applies to each connection
    });
    connect(m_rigs, &RigList::changed, this, [this] {
        updateRunning();
        maybeLoadFirst();
    });
}

QString RigSession::runningName(const QString &rigPath, const QString &rigName, const QStringList &names)
{
    const QString stem = QFileInfo(rigPath).completeBaseName();
    if (!rigPath.isEmpty() && stem != QLatin1String("last") && stem != QLatin1String("last.prev")
        && names.contains(stem))
        return stem;
    return names.contains(rigName) ? rigName : QString();
}

void RigSession::updateRunning()
{
    m_rigs->setRunning(runningName(m_engine->rigPath(), m_engine->documentName(), m_rigs->names()));
}

void RigSession::maybeLoadFirst()
{
    if (m_startupDone || m_engine->connection() != EngineSession::Connected || !m_engine->hasStatus())
        return;
    if (m_engine->hasRig()) {
        m_startupDone = true; // something is running: show it, load nothing
        return;
    }
    if (!m_rigs->loaded())
        return;
    m_startupDone = true;
    if (m_rigs->count() > 0) {
        say(tr("Nothing was running: loading %1").arg(m_rigs->nameAt(0)));
        loadRigNamed(m_rigs->nameAt(0));
    }
}

void RigSession::loadRig(int index)
{
    if (index >= 0 && index < m_rigs->count())
        loadRigNamed(m_rigs->nameAt(index));
}

void RigSession::loadRigNamed(const QString &name)
{
    if (name.isEmpty())
        return;
    m_engine->client()->request(QStringLiteral("rig.load"), {{QStringLiteral("rig"), name + QStringLiteral(".json")}},
                                [this, name](const EngineReply &reply) {
        if (!reply.ok) {
            say(tr("Couldn't load %1: %2").arg(name, reply.message.isEmpty() ? reply.code : reply.message));
            return;
        }
        const int missing = int(reply.body.value(QStringLiteral("missing_assets")).toArray().size());
        say(missing > 0 ? tr("%1: %n model(s) or IR(s) missing", nullptr, missing).arg(name) : QString());
        m_rigs->setRunning(name);
        fetchDocument();
    });
}

void RigSession::fetchDocument()
{
    if (m_fetching) {
        m_fetchAgain = true;
        return;
    }
    m_fetching = true;
    m_engine->client()->request(QStringLiteral("rig.get"), {}, [this](const EngineReply &reply) {
        m_fetching = false;
        if (reply.ok) {
            m_document = RigDocument::parse(reply.body.value(QStringLiteral("document")).toObject());
            m_missing.clear();
            for (const auto &m : reply.body.value(QStringLiteral("missing_assets")).toArray())
                m_missing.insert(m.toObject().value(QStringLiteral("node")).toString());
            m_hasDocument = true;
            emit documentChanged();
        }
        if (std::exchange(m_fetchAgain, false))
            fetchDocument();
    });
}

void RigSession::say(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
}

} // namespace rigpa
