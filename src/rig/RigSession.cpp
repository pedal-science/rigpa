#include "RigSession.h"

#include "RigEdits.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace rigpa {

RigSession::RigSession(EngineSession *engine, RigList *rigs, QObject *parent)
    : QObject(parent), m_engine(engine), m_rigs(rigs), m_edits(engine->client())
{
    // A smooth edit shows up as params_applied moving; a bypass or a structural set doesn't move
    // anything the status shows, so read the document again once the engine has answered.
    connect(&m_edits, &EditStream::answered, this, [this](const QString &kind) {
        if (kind != QLatin1String("smooth"))
            fetchDocument();
    });
    connect(&m_edits, &EditStream::refused, this, [this](const QString &message) {
        say(tr("The engine refused the edit: %1").arg(message));
        fetchDocument();
    });
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
        say(missing > 0 ? (missing == 1 ? tr("%1: a model or IR is missing").arg(name)
                                         : tr("%1: %2 models or IRs are missing").arg(name).arg(missing)) : QString());
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
        // While an apply is on its way, a read may still show the document before it: keep the
        // edited one, and read again once the engine has answered.
        if (reply.ok && m_applying == 0) {
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

namespace {

const RigElement *findIn(const QList<RigElement> &series, const QString &element)
{
    for (const RigElement &e : series) {
        if (e.element == element)
            return &e;
        for (const auto &lane : e.lanes)
            if (const RigElement *found = findIn(lane, element))
                return found;
    }
    return nullptr;
}

} // namespace

const RigElement *RigSession::findElement(const QString &element, QString *chainId) const
{
    if (element.isEmpty())
        return nullptr;
    for (const RigChain &chain : m_document.chains) {
        if (const RigElement *found = findIn(chain.series, element)) {
            if (chainId)
                *chainId = chain.id;
            return found;
        }
    }
    return nullptr;
}

bool RigSession::macroElementIsUnique(const QString &chainId, const QString &element) const
{
    const Schema &schema = m_engine->schema();
    for (const auto &c : m_document.json.value(QStringLiteral("chains")).toArray()) {
        const QJsonObject chain = c.toObject();
        if (chain.value(QStringLiteral("id")).toString() != chainId)
            continue;
        int macros = 0;
        for (const auto &n : chain.value(QStringLiteral("nodes")).toArray()) {
            const QJsonObject o = n.toObject();
            const NodeTypeSpec *t = schema.type(o.value(QStringLiteral("type")).toString());
            if (t && t->macro && o.value(QStringLiteral("element")).toString() == element)
                ++macros;
        }
        return macros == 1;
    }
    return false;
}

void RigSession::applyDocument(const RigDocument &edited)
{
    const QJsonObject json = edits::toJson(edited);
    m_document = RigDocument::parse(json);
    emit documentChanged();
    ++m_applying;
    m_engine->client()->request(QStringLiteral("rig.apply"), {{QStringLiteral("document"), json}},
                                [this](const EngineReply &reply) {
        --m_applying;
        if (!reply.ok)
            say(tr("The engine refused the change: %1").arg(reply.message.isEmpty() ? reply.code : reply.message));
        else
            say({});
        fetchDocument();
    });
}

bool RigSession::isBypassed(const QString &element) const
{
    const RigElement *e = findElement(element);
    if (!e || e->kind != RigElement::Block)
        return false;
    const Schema &schema = m_engine->schema();
    bool any = false;
    for (const RigNode &n : e->nodes) {
        const NodeTypeSpec *t = schema.type(n.type);
        if (t && !t->bypassable)
            continue;
        if (!n.bypass)
            return false;
        any = true;
    }
    return any;
}

void RigSession::setBypass(const QString &element, bool bypassed)
{
    QString chainId;
    const RigElement *e = findElement(element, &chainId);
    if (!e)
        return;
    if (e->kind != RigElement::Block) {
        say(tr("A parallel section can't be bypassed"));
        return;
    }
    const Schema &schema = m_engine->schema();
    QList<QPair<QString, QJsonObject>> ops;
    for (const RigNode &n : e->nodes) {
        const NodeTypeSpec *t = schema.type(n.type);
        if (t && !t->bypassable)
            continue;
        ops.append({n.id + QStringLiteral("#bypass"),
                    QJsonObject{{QStringLiteral("op"), QStringLiteral("bypass")},
                                {QStringLiteral("node"), n.id},
                                {QStringLiteral("bypass"), bypassed}}});
    }
    if (!ops.isEmpty())
        m_edits.submit(chainId, ops);
}

void RigSession::setParam(const QString &chainId, const RigNode &node, const QString &param, const QJsonValue &value)
{
    const NodeTypeSpec *t = m_engine->schema().type(node.type);
    const bool macro = t && t->macro;
    QJsonObject op = EditStream::target(node.id, node.element, macro,
                                        macro && macroElementIsUnique(chainId, node.element));
    op.insert(QStringLiteral("op"), QStringLiteral("set_param"));
    op.insert(QStringLiteral("param"), param);
    op.insert(QStringLiteral("value"), value);
    m_edits.submit(chainId, node.id + u'#' + param, op);
}

void RigSession::say(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
}

} // namespace rigpa
