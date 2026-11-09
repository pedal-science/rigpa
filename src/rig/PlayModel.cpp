#include "PlayModel.h"

#include "RigEditorModel.h"
#include "RigSession.h"
#include "engine/EngineSession.h"
#include "pedald/PedaldEvents.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace rigpa {

PlayModel::PlayModel(RigSession *session, RigEditorModel *editor, EngineSession *engine, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_editor(editor), m_engine(engine)
{
    connect(m_editor, &RigEditorModel::laidOut, this, &PlayModel::rebuild);
    if (m_engine) {
        connect(m_engine, &EngineSession::rigChanged, this, &PlayModel::rigChanged);
        connect(m_engine, &EngineSession::engineEvent, this, [this](const QJsonObject &frame) {
            const QString kind = frame.value(QStringLiteral("event")).toString();
            if (kind == QLatin1String("rig.params") || kind == QLatin1String("rig.patched")) {
                const QString text = describeParams(frame);
                if (!text.isEmpty())
                    setLastChange(text);
            }
        });
    }
    rebuild();
}

void PlayModel::setEvents(PedaldEvents *events)
{
    connect(events, &PedaldEvents::event, this, [this](const QString &kind, const QJsonObject &data) {
        if (kind != QLatin1String("action"))
            return;
        const QString text = describeAction(data);
        if (!text.isEmpty()) {
            // The rig it loaded is said here: the engine's report of it a moment later isn't news.
            const QString loaded = data.value(QStringLiteral("result")).toObject().value(QStringLiteral("rig")).toString();
            if (!loaded.isEmpty())
                m_rigName = loaded;
            setLastChange(text);
        }
    });
}

QString PlayModel::describeAction(const QJsonObject &event) const
{
    const QJsonObject cause = event.value(QStringLiteral("cause")).toObject();
    const QJsonObject action = event.value(QStringLiteral("action")).toObject();
    const QJsonObject result = event.value(QStringLiteral("result")).toObject();
    QString who = cause.value(QStringLiteral("label")).toString();
    if (cause.value(QStringLiteral("source")).toString() == QLatin1String("app")) {
        const QString client = cause.value(QStringLiteral("client")).toString();
        who = client.isEmpty() ? tr("an app") : client;
    }
    if (!who.isEmpty())
        who[0] = who.at(0).toUpper();
    const QString what = action.value(QStringLiteral("action")).toString();
    QString did;
    if (!result.value(QStringLiteral("ok")).toBool(true))
        did = tr("nothing (%1)").arg(result.value(QStringLiteral("reason")).toString(
            result.value(QStringLiteral("message")).toString()));
    else if (what == QLatin1String("load"))
        did = result.value(QStringLiteral("rig")).toString(action.value(QStringLiteral("rig")).toString());
    else if (what == QLatin1String("bypass"))
        did = action.value(QStringLiteral("on")).toBool() ? tr("bypass on") : tr("bypass off");
    else if (what == QLatin1String("block")) {
        QString title = result.value(QStringLiteral("element")).toString();
        for (const Chip &chip : m_chips)
            if (chip.element == title)
                title = chip.title;
        did = result.value(QStringLiteral("bypass")).toBool() ? tr("%1 off").arg(title) : tr("%1 on").arg(title);
    } else
        return {};
    return tr("%1 → %2").arg(who, did);
}

QString PlayModel::describeParams(const QJsonObject &frame) const
{
    if (!m_session || !m_session->hasDocument())
        return {};
    QJsonObject value;
    QJsonObject origin;
    if (frame.value(QStringLiteral("event")).toString() == QLatin1String("rig.patched")) {
        // A value set on a macro by its node is a structural ramp, so it comes as a patch: its last
        // set_param says the same thing a value would.
        for (const auto &op : frame.value(QStringLiteral("ops")).toArray())
            if (op.toObject().value(QStringLiteral("op")).toString() == QLatin1String("set_param"))
                value = op.toObject();
        origin = frame.value(QStringLiteral("origin")).toObject();
    } else {
        const QJsonArray values = frame.value(QStringLiteral("values")).toArray();
        if (!values.isEmpty())
            value = values.last().toObject();
        origin = value.value(QStringLiteral("origin")).toObject();
    }
    if (value.isEmpty())
        return {};
    QString client = origin.value(QStringLiteral("client")).toString();
    // pedald's own are a footswitch or MIDI, which its `action` events say better.
    if (client.isEmpty() || client == QLatin1String("pedald") || client == QLatin1String("namchaind"))
        return {};
    const QString nodeId = value.value(QStringLiteral("node")).toString();
    const QString param = value.value(QStringLiteral("param")).toString();
    QString title, paramName = param;
    for (const auto &n : m_session->document().json.value(QStringLiteral("chains")).toArray()) {
            for (const auto &node : n.toObject().value(QStringLiteral("nodes")).toArray()) {
                const QJsonObject o = node.toObject();
                if (o.value(QStringLiteral("id")).toString() != nodeId)
                    continue;
                const QString element = o.value(QStringLiteral("element")).toString();
                for (const Chip &chip : m_chips)
                    if (chip.element == element)
                        title = chip.title;
                if (m_engine)
                    if (const NodeTypeSpec *type = m_engine->schema().type(o.value(QStringLiteral("type")).toString()))
                        if (const ParamSpec *spec = type->param(param))
                            paramName = spec->name;
            }
    }
    const QJsonValue v = value.value(QStringLiteral("value"));
    const QString shown = v.isDouble() ? QString::number(v.toDouble(), 'f', 1) : v.toVariant().toString();
    client[0] = client.at(0).toUpper();
    return title.isEmpty() ? tr("%1: %2 %3").arg(client, paramName, shown)
                           : tr("%1: %2 · %3 %4").arg(client, title, paramName, shown);
}

void PlayModel::rebuild()
{
    QList<std::pair<int, Chip>> blocks;
    for (const RigEditorModel::Item &item : m_editor->items()) {
        // The lifted element, mid-drag, is the editor's business: the play view isn't showing then.
        if (item.kind != QLatin1String("block") || item.select < 0 || item.lift != 0)
            continue;
        Chip chip;
        chip.element = item.element;
        chip.title = item.title;
        chip.subtitle = item.subtitle;
        chip.color = item.color;
        chip.bypassed = item.bypassed;
        chip.missing = item.missing;
        blocks.append({item.select, chip});
    }
    std::stable_sort(blocks.begin(), blocks.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    QList<Chip> chips;
    for (const auto &[select, chip] : blocks)
        chips.append(chip);
    if (chips == m_chips)
        return;

    if (chips.size() == m_chips.size()) {
        // The same blocks, one of them switched: a change of data, not a new list, so a chip that
        // was just tapped (or highlighted by the rotary) stays where it is.
        m_chips = chips;
        emit dataChanged(index(0), index(int(m_chips.size()) - 1));
    } else {
        beginResetModel();
        m_chips = chips;
        endResetModel();
    }
    emit changed();
}

void PlayModel::rigChanged()
{
    const QString name = m_engine->rigName();
    if (name == m_rigName)
        return;
    const bool first = m_rigName.isEmpty();
    m_rigName = name;
    if (!first && !name.isEmpty())
        setLastChange(tr("Now playing %1").arg(name));
}

QString PlayModel::elementAt(int row) const
{
    return row >= 0 && row < m_chips.size() ? m_chips.at(row).element : QString();
}

void PlayModel::toggle(int row)
{
    if (!m_session || row < 0 || row >= m_chips.size())
        return;
    const Chip &chip = m_chips.at(row);
    const bool bypass = !m_session->isBypassed(chip.element);
    m_session->setBypass(chip.element, bypass);
    setLastChange(bypass ? tr("%1 off").arg(chip.title) : tr("%1 on").arg(chip.title));
}

void PlayModel::setLastChange(const QString &text)
{
    if (text == m_lastChange)
        return;
    m_lastChange = text;
    emit lastChangeChanged();
}

int PlayModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_chips.size());
}

QVariant PlayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_chips.size())
        return {};
    const Chip &chip = m_chips.at(index.row());
    switch (role) {
    case ElementRole: return chip.element;
    case TitleRole: return chip.title;
    case SubtitleRole: return chip.subtitle;
    case ColorRole: return chip.color;
    case BypassedRole: return chip.bypassed;
    case MissingRole: return chip.missing;
    }
    return {};
}

QHash<int, QByteArray> PlayModel::roleNames() const
{
    return {
        {ElementRole, "element"}, {TitleRole, "title"}, {SubtitleRole, "subtitle"},
        {ColorRole, "chipColor"}, {BypassedRole, "bypassed"}, {MissingRole, "missing"},
    };
}

} // namespace rigpa
