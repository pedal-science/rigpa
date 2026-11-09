#include "RigSession.h"

#include "RigEdits.h"
#include "pedald/PedaldClient.h"
#include "pedald/PedaldEvents.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>
#include <QJsonArray>
#include <QJsonObject>

namespace rigpa {

RigSession::RigSession(EngineSession *engine, RigList *rigs, QObject *parent)
    : QObject(parent), m_engine(engine), m_rigs(rigs), m_edits(engine->client())
{
    // A smooth edit shows up as params_applied moving; a bypass or a structural set doesn't move
    // anything the status shows, so read the document again once the engine has answered.
    connect(&m_edits, &EditStream::answered, this, [this](const QString &kind, qint64 seq) {
        if (seq > m_seq)
            m_seq = seq;
        if (kind != QLatin1String("smooth"))
            fetchDocument();
        else if (m_engine->following())
            m_settle.start(); // the engine doesn't tell a client of its own values
    });
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);
    connect(&m_settle, &QTimer::timeout, this, &RigSession::fetchDocument);
    connect(m_engine, &EngineSession::engineEvent, this, &RigSession::onEngineEvent);
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
        // A smoothed knob moves params_applied without a new generation: re-read for that too,
        // unless the engine is pushing the values themselves.
        if (!m_engine->following() && m_engine->paramsApplied() != m_seenParams && m_engine->hasRig()) {
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
    // While an apply is on its way the engine's status still names the rig before it (a rename
    // would read as "not in the library" for a moment); the first status after it settles it.
    if (m_applying > 0)
        return;
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

void RigSession::setPedal(PedaldClient *pedald, PedaldEvents *events)
{
    m_pedald = pedald;
    m_pedalEvents = events;
    connect(events, &PedaldEvents::event, this, &RigSession::onPedalEvent);
    connect(events, &PedaldEvents::connectedChanged, this, [this] {
        emit historyChanged();
        if (m_pedalEvents->connected())
            readPedalHistory();
    });
    connect(m_engine, &EngineSession::rigChanged, this, &RigSession::readPedalHistory);
}

bool RigSession::pedalUndo() const
{
    return m_pedalEvents && m_pedalEvents->connected();
}

void RigSession::readPedalHistory()
{
    if (!pedalUndo())
        return;
    m_pedald->get(QStringLiteral("/running"), [this](const PedaldReply &reply) {
        if (!reply.ok)
            return;
        const QJsonObject running = reply.body.object();
        m_pedalUndo = running.value(QStringLiteral("undo")).toInt();
        m_pedalRedo = running.value(QStringLiteral("redo")).toInt();
        emit historyChanged();
    });
}

void RigSession::onPedalEvent(const QString &kind, const QJsonObject &data)
{
    if (kind == QLatin1String("history")) {
        m_pedalUndo = data.value(QStringLiteral("undo")).toInt();
        m_pedalRedo = data.value(QStringLiteral("redo")).toInt();
        emit historyChanged();
    } else if (kind == QLatin1String("hello")) {
        readPedalHistory();
    }
}

void RigSession::onEngineEvent(const QJsonObject &frame)
{
    const QString kind = frame.value(QStringLiteral("event")).toString();
    if (kind == QLatin1String("resync") || kind == QLatin1String("rig.replaced")) {
        fetchDocument();
        return;
    }
    const qint64 seq = frame.value(QStringLiteral("seq")).toInteger(-1);
    if (kind == QLatin1String("rig.patched")) {
        if (seq >= 0 && seq <= m_seq)
            return;
        // Our own apply is on its way: the engine's document after both is what to show.
        if (m_applying > 0 || !m_hasDocument) {
            fetchDocument();
            return;
        }
        m_seq = seq;
        adopt(frame.value(QStringLiteral("document")).toObject());
        return;
    }
    if (kind != QLatin1String("rig.params") || !m_hasDocument)
        return;
    QJsonObject json = m_document.json;
    QJsonArray chains = json.value(QStringLiteral("chains")).toArray();
    bool changed = false;
    for (const auto &v : frame.value(QStringLiteral("values")).toArray()) {
        const QJsonObject value = v.toObject();
        const qint64 valueSeq = value.value(QStringLiteral("seq")).toInteger(-1);
        if (valueSeq >= 0 && valueSeq <= m_seq)
            continue;
        if (valueSeq > m_seq)
            m_seq = valueSeq;
        const QString node = value.value(QStringLiteral("node")).toString();
        if (node.isEmpty())
            continue; // runtime state, not in the document
        const QString chainId = value.value(QStringLiteral("chain")).toString();
        for (int c = 0; c < chains.size(); ++c) {
            QJsonObject chain = chains.at(c).toObject();
            if (!chainId.isEmpty() && chain.value(QStringLiteral("id")).toString() != chainId)
                continue;
            QJsonArray nodes = chain.value(QStringLiteral("nodes")).toArray();
            for (int n = 0; n < nodes.size(); ++n) {
                QJsonObject o = nodes.at(n).toObject();
                if (o.value(QStringLiteral("id")).toString() != node)
                    continue;
                o.insert(value.value(QStringLiteral("param")).toString(), value.value(QStringLiteral("value")));
                nodes.replace(n, o);
                changed = true;
            }
            chain.insert(QStringLiteral("nodes"), nodes);
            chains.replace(c, chain);
        }
    }
    if (!changed)
        return;
    json.insert(QStringLiteral("chains"), chains);
    m_document = RigDocument::parse(json);
    emit documentChanged();
}

void RigSession::adopt(QJsonObject running)
{
    edits::stripTuner(running);
    const QString before = m_document.name;
    m_document = RigDocument::parse(running);
    if (m_hasDocument && m_document.name != before && !pedalUndo())
        clearHistory();
    m_hasDocument = true;
    emit documentChanged();
}

void RigSession::startGesture()
{
    m_gesture = QStringLiteral("gesture:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_edits.setTag(m_gesture);
}

void RigSession::loadRigNamed(const QString &name)
{
    if (name.isEmpty())
        return;
    clearHistory();
    if (m_pedald && m_pedald->configured()) {
        // The footswitch's path (pedal-service.md, "Keeping the rig"): the pedal moves its bank
        // position and warms the next rig, and every view is told the app did it.
        const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("rig"), name}}).toJson(QJsonDocument::Compact);
        m_pedald->put(QStringLiteral("/running"), body, [this, name](const PedaldReply &reply) {
            if (!reply.ok) {
                say(tr("Couldn't load %1: %2").arg(name, reply.error));
                return;
            }
            say({});
            m_rigs->setRunning(name);
            fetchDocument();
        });
        return;
    }
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
            const QString before = m_document.name;
            // The engine's copy carries the tuner node while the tuner is up (this session put it
            // there). It is not part of the rig: strip it before the document becomes the model, or
            // the canvas would draw it, an undo step would keep it and a save would write it.
            QJsonObject running = reply.body.value(QStringLiteral("document")).toObject();
            const qint64 seq = reply.body.value(QStringLiteral("seq")).toInteger(-1);
            if (seq >= 0)
                m_seq = seq;
            edits::stripTuner(running);
            m_document = RigDocument::parse(running);
            if (m_hasDocument && m_document.name != before && !pedalUndo())
                clearHistory(); // another rig: its own history
            m_missing.clear();
            for (const auto &m : reply.body.value(QStringLiteral("missing_assets")).toArray())
                m_missing.insert(m.toObject().value(QStringLiteral("node")).toString());
            m_hasDocument = true;
            emit documentChanged();
            // The view is up but the engine is not running the node -- the app started with
            // `--view tuner` before the document arrived, a footswitch loaded a rig through pedald,
            // or a mount was refused. Mount it, once per generation: this is what makes "while the
            // tuner is on screen the rig is silent" true even when the document came from elsewhere.
            if (m_tuner && !edits::hasTuner(running) && m_engine->generation() != m_tunerTried) {
                m_tunerTried = m_engine->generation();
                publishRun();
            }
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

void RigSession::record()
{
    if (!m_hasDocument)
        return;
    if (pedalUndo()) {
        m_lastParamKey.clear();
        emit edited();
        return;
    }
    m_undo.append(m_document.json);
    while (m_undo.size() > kHistoryLimit)
        m_undo.removeFirst();
    m_redo.clear();
    m_lastParamKey.clear();
    emit historyChanged();
    emit edited();
}

void RigSession::clearHistory()
{
    if (m_undo.isEmpty() && m_redo.isEmpty())
        return;
    m_undo.clear();
    m_redo.clear();
    m_lastParamKey.clear();
    emit historyChanged();
}

void RigSession::stepPedal(bool forwards)
{
    m_pedald->post(forwards ? QStringLiteral("/redo") : QStringLiteral("/undo"), QByteArray(""),
                   [this, forwards](const PedaldReply &reply) {
        if (!reply.ok) {
            say(forwards ? tr("Nothing was redone: %1").arg(reply.error) : tr("Nothing was undone: %1").arg(reply.error));
            readPedalHistory();
            return;
        }
        say({});
        m_pedalUndo = reply.body.object().value(QStringLiteral("undo")).toInt();
        m_pedalRedo = reply.body.object().value(QStringLiteral("redo")).toInt();
        emit historyChanged();
        fetchDocument();
    });
}

void RigSession::undo()
{
    if (pedalUndo()) {
        stepPedal(false);
        return;
    }
    if (m_undo.isEmpty() || !m_hasDocument)
        return;
    m_redo.append(m_document.json);
    const QJsonObject before = m_undo.takeLast();
    m_lastParamKey.clear();
    emit historyChanged();
    emit edited();
    applyJson(before);
}

void RigSession::redo()
{
    if (pedalUndo()) {
        stepPedal(true);
        return;
    }
    if (m_redo.isEmpty() || !m_hasDocument)
        return;
    m_undo.append(m_document.json);
    const QJsonObject after = m_redo.takeLast();
    m_lastParamKey.clear();
    emit historyChanged();
    emit edited();
    applyJson(after);
}

void RigSession::applyDocument(const RigDocument &edited)
{
    record();
    startGesture();
    applyJson(edits::toJson(edited), true);
}

void RigSession::applyJson(const QJsonObject &json, bool recorded)
{
    m_document = RigDocument::parse(json);
    emit documentChanged();
    // What goes on the wire is the document plus the tuner's node while the tuner is up, so an edit
    // made with a tuner on screen does not un-mute the rig: the player would not hear the change
    // they are making, and a click on the amp is exactly what a tuner exists to avoid.
    QJsonObject published = json;
    if (m_tuner)
        edits::injectTuner(published, m_tunerMuted);
    ++m_applying;
    QJsonObject params{{QStringLiteral("document"), published}};
    if (recorded) {
        // An edit: one gesture, made from the document at `m_seq`. If somebody else's change has
        // landed since, the engine refuses it rather than this undoing theirs.
        params.insert(QStringLiteral("tag"), m_gesture);
        // Only while nothing of ours is on its way: our own change ahead of this one would move the
        // document first, and this would be refused for it.
        if (m_seq >= 0 && !m_edits.busy() && m_applying == 0)
            params.insert(QStringLiteral("if_seq"), m_seq);
    }
    m_engine->client()->request(QStringLiteral("rig.apply"), params, [this, recorded](const EngineReply &reply) {
        if (!reply.ok && recorded && !m_undo.isEmpty() && !pedalUndo()) {
            m_undo.removeLast(); // nothing changed, so there is nothing to undo
            emit historyChanged();
        }
        --m_applying;
        if (reply.ok && reply.body.value(QStringLiteral("seq")).toInteger(-1) > m_seq)
            m_seq = reply.body.value(QStringLiteral("seq")).toInteger();
        if (reply.ok)
            say({});
        else if (reply.code == QLatin1String("conflict"))
            say(tr("Someone else changed the rig first: showing theirs"));
        else if (reply.code == QLatin1String("over_budget"))
            // The engine lists every unit's cost after the sentence that matters.
            say(tr("Too much for the pedal: %1").arg(reply.message.section(QStringLiteral(" ("), 0, 0)));
        else
            say(tr("The engine refused the change: %1").arg(reply.message.isEmpty() ? reply.code : reply.message));
        fetchDocument();
    });
}

void RigSession::setTuner(bool on)
{
    if (on == m_tuner)
        return;
    m_tuner = on;
    m_tunerTried = -1;
    emit tunerChanged();
    publishRun();
}

void RigSession::publishRun()
{
    if (!m_hasDocument || m_document.chains.isEmpty())
        return;
    QJsonObject published = edits::toJson(m_document);
    // With the node when the tuner is on, without it when it is off: either way the engine is left
    // running exactly what the view says it is.
    if (m_tuner)
        edits::injectTuner(published, m_tunerMuted);
    ++m_applying;
    m_engine->client()->request(QStringLiteral("rig.apply"), {{QStringLiteral("document"), published}},
                                [this](const EngineReply &reply) {
        --m_applying;
        if (reply.ok && reply.body.value(QStringLiteral("seq")).toInteger(-1) > m_seq)
            m_seq = reply.body.value(QStringLiteral("seq")).toInteger();
        if (!reply.ok)
            say(tr("The tuner could not be %1: %2")
                    .arg(m_tuner ? tr("mounted") : tr("removed"),
                         reply.message.isEmpty() ? reply.code : reply.message));
        fetchDocument();
    });
}

void RigSession::setTunerMuted(bool muted)
{
    if (muted == m_tunerMuted)
        return;
    // Without a mounted node there is nothing to patch: the switch is remembered, and the node is
    // mounted with it the next time the view opens.
    if (!m_tuner || !m_hasDocument || m_document.chains.isEmpty()) {
        m_tunerMuted = muted;
        emit tunerMutedChanged();
        return;
    }
    QJsonArray ops{QJsonObject{{QStringLiteral("op"), QStringLiteral("set_param")},
                              {QStringLiteral("node"), QStringLiteral("tuner")},
                              {QStringLiteral("param"), QStringLiteral("mute")},
                              {QStringLiteral("value"), muted ? 1 : 0}}};
    m_tunerMuted = muted;
    emit tunerMutedChanged();
    // One patch per chain: `rig.patch` names the chain it applies to, and the node's id is the same
    // in each. Three at most, and each answer is the same command queue, so a failure on one is a
    // failure on all.
    for (const RigChain &chain : m_document.chains) {
        const QString id = chain.id;
        if (id.isEmpty())
            continue;
        m_engine->client()->request(
            QStringLiteral("rig.patch"),
            {{QStringLiteral("chain"), id},
             {QStringLiteral("ops"), ops}},
            [this, muted](const EngineReply &reply) {
                if (reply.ok && reply.body.value(QStringLiteral("seq")).toInteger(-1) > m_seq)
                    m_seq = reply.body.value(QStringLiteral("seq")).toInteger();
                if (reply.ok)
                    return;
                say(tr("The tuner's switch did not move: %1")
                        .arg(reply.message.isEmpty() ? reply.code : reply.message));
                // Put it back: the node did not move, so neither should the button.
                m_tunerMuted = !muted;
                emit tunerMutedChanged();
            });
    }
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
    if (!ops.isEmpty()) {
        record();
        startGesture();
        m_edits.submit(chainId, ops);
    }
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
    const QString key = node.id + u'#' + param;
    // One step per run of changes to one parameter.
    if (key != m_lastParamKey || !m_lastParamAt.isValid() || m_lastParamAt.elapsed() > kParamStepMs) {
        record();
        startGesture();
        m_lastParamKey = key;
    }
    m_lastParamAt.start();
    m_edits.submit(chainId, key, op);
}

void RigSession::say(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
}

} // namespace rigpa
