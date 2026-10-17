#include "NodeEditorModel.h"

#include "RigEdits.h"
#include "RigEditorModel.h"
#include "RigSession.h"
#include "engine/EngineSession.h"
#include "library/Library.h"

#include <QJsonValue>
#include <QSet>

#include <algorithm>

#include <cmath>

namespace rigpa {

NodeEditorModel::NodeEditorModel(RigSession *session, EngineSession *engine, RigEditorModel *editor,
                                 Library *library, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_engine(engine), m_editor(editor), m_library(library)
{
    m_flow = new FlowEqModel(this);
    if (m_engine)
        m_flow->setClient(m_engine->client());
    m_reread.setSingleShot(true);
    connect(&m_reread, &QTimer::timeout, this, [this] {
        if (!isOpen())
            return;
        readValues(true);
        if (!m_controls.isEmpty())
            emit dataChanged(index(0), index(int(m_controls.size()) - 1));
        emit changed();
    });
    connect(m_session, &RigSession::documentChanged, this, [this] {
        if (!isOpen())
            return;
        QString chain;
        const RigElement *e = m_session->findElement(m_element, &chain);
        if (!e || shapeOf(*e) != m_shape || chain != m_chain) {
            rebuild();
            return;
        }
        readValues(true);
        scheduleReread();
        if (m_isFlowEq && m_flow)
            m_flow->noteEdit(); // someone else moved the block: the curve they hear is an answer
        if (!m_controls.isEmpty())
            emit dataChanged(index(0), index(int(m_controls.size()) - 1));
        emit changed();
    });
    if (m_engine)
        connect(m_engine, &EngineSession::schemaChanged, this, [this] { if (isOpen()) rebuild(); });
    if (m_editor)
        connect(m_editor, &RigEditorModel::laidOut, this, [this] {
            if (!isOpen())
                return;
            if (const auto *item = m_editor->itemFor(m_element)) {
                m_title = item->title;
                m_subtitle = item->subtitle;
                emit changed();
            }
        });

    connect(&m_ring, &FocusRing::stepped, this, &NodeEditorModel::step);
    connect(&m_ring, &FocusRing::toggled, this, [this](int i) {
        if (i == bypassIndex()) {
            toggleBypass();
            return;
        }
        if (i < 0 || i >= m_controls.size())
            return;
        const Control &c = m_controls.at(i);
        if (c.kind == QLatin1String("choice"))
            choose(i, (int(c.value) + 1) % int(c.spec.options.size()));
        else if (c.kind == QLatin1String("toggle"))
            choose(i, c.value != 0 ? 0 : 1);
    });
    connect(&m_ring, &FocusRing::activated, this, [this](int i) {
        if (i == closeIndex())
            emit closeRequested();
        else if (i == removeIndex())
            removeElement();
        else if (i >= 0 && i < m_controls.size() && m_controls.at(i).kind == QLatin1String("picker"))
            emit pickerRequested(i);
    });
    // The rotary changes a selected value; what the engine doesn't ramp is sent when it is let go.
    connect(&m_ring, &FocusRing::editingChanged, this, [this] {
        if (!m_ring.editing() && m_lastEdited >= 0)
            release(m_lastEdited);
    });
}

bool NodeEditorModel::bypassed() const
{
    return isOpen() && m_session->isBypassed(m_element);
}

void NodeEditorModel::open(const QString &element)
{
    if (element.isEmpty()) {
        close();
        return;
    }
    const bool was = isOpen();
    m_element = element;
    rebuild();
    m_ring.setEditing(false);
    m_ring.setHighlight(0);
    if (!was || isOpen())
        emit openChanged();
}

void NodeEditorModel::close()
{
    if (!isOpen())
        return;
    if (m_lastEdited >= 0)
        release(m_lastEdited);
    m_ring.setEditing(false);
    m_reread.stop();
    if (m_flow)
        m_flow->watch(QString());
    beginResetModel();
    m_element.clear();
    m_chain.clear();
    m_controls.clear();
    m_shape.clear();
    m_isFlowEq = false;
    m_lastEdited = -1;
    endResetModel();
    updateRing();
    emit openChanged();
    emit changed();
}

void NodeEditorModel::rebuild()
{
    beginResetModel();
    m_controls.clear();
    m_shape.clear();
    m_note.clear();
    m_bypassable = false;
    m_lastEdited = -1;

    const RigElement *e = m_session->findElement(m_element, &m_chain);
    if (!e) {
        endResetModel();
        close();
        return;
    }
    m_shape = shapeOf(*e);
    // The one block that gets its own panel: nc.eq.flow is a single node, and the engine reports
    // what it derived (FlowEqModel). A lane or a section never is.
    m_isFlowEq = e->kind == RigElement::Block && e->nodes.size() == 1
                 && e->nodes.first().type == QLatin1String("nc.eq.flow");
    if (m_flow)
        m_flow->watch(m_isFlowEq ? m_element : QString());
    if (const auto *item = m_editor ? m_editor->itemFor(m_element) : nullptr) {
        m_title = item->title;
        m_subtitle = item->subtitle;
    } else {
        m_title = m_element;
        m_subtitle.clear();
    }

    const Schema &schema = m_engine->schema();
    if (e->kind == RigElement::Section) {
        buildSection(*e);
    } else if (schema.isEmpty()) {
        m_note = tr("Waiting for the engine's catalogue of parameters…");
    }

    if (e->kind == RigElement::Block) {
        for (const RigNode &n : e->nodes) {
            const NodeTypeSpec *t = schema.type(n.type);
            if (!t)
                continue;
            m_bypassable = m_bypassable || t->bypassable;
            bool first = true;
            QSet<QString> groupsDone;
            for (const ParamSpec &p : t->params) {
                if (p.hidden || !p.patchable || p.kind == ParamSpec::Other)
                    continue;
                // Alternatives (a delay in samples or in ms): show the one the node sets, else
                // the one in milliseconds, else the first.
                if (!p.oneOf.isEmpty()) {
                    if (groupsDone.contains(p.oneOf))
                        continue;
                    const ParamSpec *pick = nullptr;
                    for (const ParamSpec &alt : t->params)
                        if (alt.oneOf == p.oneOf && n.json.contains(alt.id))
                            pick = &alt;
                    if (!pick)
                        for (const ParamSpec &alt : t->params)
                            if (alt.oneOf == p.oneOf && alt.unit == QLatin1String("ms"))
                                pick = &alt;
                    if (pick && pick->id != p.id)
                        continue;
                    groupsDone.insert(p.oneOf);
                }
                Control c;
                c.node = n;
                c.spec = p;
                c.group = t->name;
                c.firstOfGroup = first;
                first = false;
                if (p.kind == ParamSpec::Flag)
                    c.kind = QStringLiteral("toggle");
                else if (p.kind == ParamSpec::Option && !p.optionsFrom.isEmpty())
                    c.kind = QStringLiteral("picker");
                else if (p.kind == ParamSpec::Option && p.options.size() <= kMaxSegments)
                    c.kind = QStringLiteral("choice");
                else
                    c.kind = QStringLiteral("knob");
                if (p.kind == ParamSpec::Option && p.optionsFrom.isEmpty() && p.options.isEmpty())
                    continue;
                m_controls.append(c);
            }
        }
        if (m_controls.isEmpty() && m_note.isEmpty())
            m_note = tr("Nothing to adjust here.");
    }
    readValues(false);
    endResetModel();
    updateRing();
    emit changed();
}

QStringList NodeEditorModel::shapeOf(const RigElement &e)
{
    QStringList shape;
    for (const RigNode &n : e.nodes)
        shape << n.id + u'=' + n.type;
    if (e.kind == RigElement::Section)
        shape << QStringLiteral("lanes=%1").arg(e.lanes.size());
    return shape;
}

void NodeEditorModel::buildSection(const RigElement &e)
{
    // Lanes, then each lane's level, mute and polarity.
    Control lanes;
    lanes.kind = QStringLiteral("choice");
    lanes.lanePart = QStringLiteral("lanes");
    lanes.group = tr("Parallel");
    lanes.firstOfGroup = true;
    lanes.spec.id = QStringLiteral("lanes");
    lanes.spec.name = tr("Lanes");
    lanes.spec.kind = ParamSpec::Option;
    const int count = int(e.lanes.size());
    for (int n = std::min(2, count); n <= std::max(4, count); ++n)
        lanes.spec.options.append({QString::number(n), QString::number(n)});
    m_controls.append(lanes);
    for (int l = 0; l < count; ++l) {
        Control level;
        level.kind = QStringLiteral("knob");
        level.lanePart = QStringLiteral("level");
        level.lane = l;
        level.group = tr("Lane %1").arg(l + 1);
        level.firstOfGroup = true;
        level.spec.id = QStringLiteral("level");
        level.spec.name = tr("Level");
        level.spec.kind = ParamSpec::Number;
        level.spec.min = -40;
        level.spec.max = 12;
        level.spec.hasDefault = true;
        level.spec.unit = QStringLiteral("dB");
        level.spec.rampable = true;
        m_controls.append(level);
        const QList<std::pair<QString, QString>> flags{{QStringLiteral("mute"), tr("Mute")},
                                                       {QStringLiteral("invert"), tr("Invert")}};
        for (const auto &[part, name] : flags) {
            Control t;
            t.kind = QStringLiteral("toggle");
            t.lanePart = part;
            t.lane = l;
            t.group = level.group;
            t.spec.id = part;
            t.spec.name = name;
            t.spec.kind = ParamSpec::Flag;
            m_controls.append(t);
        }
    }
}

void NodeEditorModel::readLaneValue(Control &c, const RigElement &section, bool keepHeld) const
{
    if (keepHeld && (c.dirty || (c.held.isValid() && c.held.elapsed() < kHoldMs)))
        return;
    if (c.lanePart == QLatin1String("lanes")) {
        const QString n = QString::number(section.lanes.size());
        for (int i = 0; i < c.spec.options.size(); ++i)
            if (c.spec.options.at(i).value == n)
                c.value = i;
        return;
    }
    const edits::LaneGain g = edits::laneGainOf(section, c.lane);
    if (c.lanePart == QLatin1String("level"))
        c.value = std::round(g.levelDb * 100) / 100;
    else if (c.lanePart == QLatin1String("mute"))
        c.value = g.mute ? 1 : 0;
    else
        c.value = g.invert ? 1 : 0;
}

void NodeEditorModel::sendLane(Control &c)
{
    c.dirty = false;
    c.held.start();
    RigDocument doc = m_session->document();
    const RigElement *section = edits::find(doc, m_element);
    if (!section)
        return;
    if (c.lanePart == QLatin1String("lanes")) {
        if (edits::setLaneCount(doc, m_element, c.spec.options.value(int(c.value)).value.toInt()))
            m_session->applyDocument(doc);
        return;
    }
    edits::LaneGain g = edits::laneGainOf(*section, c.lane);
    const RigNode node = section->laneGains.value(c.lane);
    if (c.lanePart == QLatin1String("level")) {
        g.levelDb = c.value;
        // The lane has its gain node already: ramp it there, as any knob.
        if (!node.id.isEmpty() && !g.mute) {
            m_session->setParam(m_chain, node, QStringLiteral("gain"), g.linear());
            return;
        }
    } else if (c.lanePart == QLatin1String("mute")) {
        g.mute = c.value != 0;
    } else {
        g.invert = c.value != 0;
    }
    if (edits::setLaneGain(doc, m_element, c.lane, g))
        m_session->applyDocument(doc);
}

void NodeEditorModel::removeElement()
{
    if (!isOpen())
        return;
    RigDocument doc = m_session->document();
    if (edits::remove(doc, m_element))
        m_session->applyDocument(doc); // the element goes, and the editor closes with it
}

void NodeEditorModel::readValues(bool keepHeld)
{
    const RigElement *e = m_session->findElement(m_element);
    if (!e)
        return;
    if (e->kind == RigElement::Section) {
        for (Control &c : m_controls)
            readLaneValue(c, *e, keepHeld);
        return;
    }
    for (Control &c : m_controls) {
        for (const RigNode &n : e->nodes)
            if (n.id == c.node.id)
                c.node = n;
        readValue(c, keepHeld);
    }
}

void NodeEditorModel::readValue(Control &c, bool keepHeld) const
{
    if (keepHeld && (c.dirty || (c.held.isValid() && c.held.elapsed() < kHoldMs)))
        return;
    const QJsonValue v = c.node.json.value(c.spec.id);
    switch (c.spec.kind) {
    case ParamSpec::Number:
        c.value = v.isDouble() ? v.toDouble() : c.spec.hasDefault ? c.spec.defaultValue : Schema::lowerOf(c.spec);
        break;
    case ParamSpec::Flag:
        c.value = v.isBool() ? (v.toBool() ? 1 : 0) : v.isDouble() ? (v.toDouble() != 0 ? 1 : 0) : c.spec.defaultValue;
        break;
    case ParamSpec::Option:
        if (!c.spec.optionsFrom.isEmpty()) {
            c.ref = v.toString();
            break;
        }
        c.value = c.spec.hasDefault ? c.spec.defaultValue : 0;
        if (!v.isUndefined() && !v.isNull()) {
            const QString s = v.isDouble() ? QString::number(v.toDouble()) : v.toString();
            for (int i = 0; i < c.spec.options.size(); ++i)
                if (c.spec.options.at(i).value == s)
                    c.value = i;
        }
        break;
    case ParamSpec::Other:
        break;
    }
}

void NodeEditorModel::scheduleReread()
{
    // A control that was moved here keeps its own value for kHoldMs, so it does not jump back to an
    // older reading while a knob is being dragged. The catch: a document change that lands inside
    // that window is skipped for that control, and the document then stops changing -- so an edit
    // made somewhere else (a footswitch, ljoss, another client) would stay invisible on it until the
    // next change anywhere. One delayed read closes that, and also picks up a value the engine
    // clamped or rounded on its way through a patch.
    qint64 remaining = 0;
    for (const Control &c : m_controls) {
        if (c.held.isValid() && c.held.elapsed() < kHoldMs)
            remaining = std::max(remaining, qint64(kHoldMs) - c.held.elapsed());
    }
    if (remaining > 0)
        m_reread.start(int(remaining) + 50);
}

void NodeEditorModel::updateRing()
{
    QStringList kinds;
    for (const Control &c : m_controls) {
        if (c.kind == QLatin1String("knob"))
            kinds << QStringLiteral("value");
        else if (c.kind == QLatin1String("picker"))
            kinds << QStringLiteral("action");
        else
            kinds << QStringLiteral("toggle");
    }
    if (m_bypassable)
        kinds << QStringLiteral("toggle");
    kinds << QStringLiteral("action"); // Close
    kinds << QStringLiteral("action"); // Remove
    m_ring.setKinds(kinds);
    m_ring.setCount(isOpen() ? int(kinds.size()) : 0);
}

bool NodeEditorModel::streams(const Control &c) const
{
    if (c.lanePart == QLatin1String("level")) {
        // Only once the lane has a gain node to ramp; before that the first value adds one.
        const RigElement *section = m_session->findElement(m_element);
        return section && !section->laneGains.value(c.lane).id.isEmpty() && !edits::laneGainOf(*section, c.lane).mute;
    }
    return c.kind == QLatin1String("knob") && c.spec.kind == ParamSpec::Number && c.spec.rampable;
}

void NodeEditorModel::touch(int index)
{
    m_controls[index].held.start();
    m_lastEdited = index;
    m_ring.setHighlight(index);
}

void NodeEditorModel::setValue(int index, double value)
{
    Control &c = m_controls[index];
    if (c.spec.kind == ParamSpec::Number) {
        if (c.spec.step <= 0)
            value = std::round(value * 1e4) / 1e4; // no 4.800000000000007 in a saved rig
        // After the rounding, so an end the engine excludes can't be rounded back onto.
        value = std::clamp(value, Schema::lowerOf(c.spec), Schema::upperOf(c.spec));
    }
    else if (c.spec.kind == ParamSpec::Option)
        value = std::clamp(std::round(value), 0.0, double(c.spec.options.size() - 1));
    touch(index);
    if (value == c.value)
        return;
    c.value = value;
    emit dataChanged(this->index(index), this->index(index));
    if (streams(c))
        send(index);
    else
        c.dirty = true;
}

void NodeEditorModel::setPosition(int index, double position)
{
    if (index < 0 || index >= m_controls.size())
        return;
    const Control &c = m_controls.at(index);
    position = std::clamp(position, 0.0, 1.0);
    if (c.spec.kind == ParamSpec::Option)
        setValue(index, position * (c.spec.options.size() - 1));
    else if (c.spec.kind == ParamSpec::Number)
        setValue(index, valueAt(c.spec, position));
}

void NodeEditorModel::release(int index)
{
    if (index < 0 || index >= m_controls.size())
        return;
    if (m_controls.at(index).dirty)
        send(index);
}

void NodeEditorModel::resetToDefault(int index)
{
    if (index < 0 || index >= m_controls.size())
        return;
    const ParamSpec &p = m_controls.at(index).spec;
    if (p.kind != ParamSpec::Number && !(p.kind == ParamSpec::Option && p.optionsFrom.isEmpty()))
        return;
    setValue(index, p.hasDefault ? p.defaultValue : Schema::lowerOf(p));
    release(index);
}

void NodeEditorModel::step(int index, int steps)
{
    if (index < 0 || index >= m_controls.size() || steps == 0)
        return;
    const Control &c = m_controls.at(index);
    if (c.spec.kind == ParamSpec::Option) {
        setValue(index, c.value + steps);
    } else if (c.spec.kind == ParamSpec::Number) {
        const double lo = Schema::lowerOf(c.spec), hi = Schema::upperOf(c.spec);
        if (c.spec.step > 0) {
            // A stepped parameter moves by its step, or by enough steps to cross the range in
            // about kDetents detents.
            const double unit = c.spec.step * std::max(1.0, std::round((hi - lo) / kDetents / c.spec.step));
            setValue(index, c.value + steps * unit);
        } else {
            setValue(index, valueAt(c.spec, positionOf(c.spec, c.value) + double(steps) / kDetents));
        }
    }
}

void NodeEditorModel::choose(int index, int option)
{
    if (index < 0 || index >= m_controls.size())
        return;
    setValue(index, option);
    release(index);
}

void NodeEditorModel::toggleBypass()
{
    if (isOpen() && m_bypassable)
        m_session->toggleBypass(m_element);
}

void NodeEditorModel::pick(int index, const QString &id)
{
    if (index < 0 || index >= m_controls.size() || id.isEmpty())
        return;
    Control &c = m_controls[index];
    touch(index);
    if (c.ref == id)
        return;
    c.ref = id;
    emit dataChanged(this->index(index), this->index(index));
    send(index);
}

int NodeEditorModel::pickerKind(int index) const
{
    if (index < 0 || index >= m_controls.size())
        return 0;
    return m_controls.at(index).spec.optionsFrom == QLatin1String("irs") ? LibraryEntry::Ir : LibraryEntry::Model;
}

QString NodeEditorModel::pickerCurrent(int index) const
{
    return index >= 0 && index < m_controls.size() ? m_controls.at(index).ref : QString();
}

void NodeEditorModel::send(int index)
{
    Control &c = m_controls[index];
    if (!c.lanePart.isEmpty()) {
        sendLane(c);
        return;
    }
    c.dirty = false;
    c.held.start();
    if (m_isFlowEq && m_flow)
        m_flow->noteEdit(); // the engine's answer to this edit is what the curve draws
    QJsonValue value;
    switch (c.spec.kind) {
    case ParamSpec::Number:
        value = c.value;
        break;
    case ParamSpec::Flag:
        // A patch takes a number or a string, never a boolean (namchain graph/patch.cpp): a flag is
        // 0 or 1 on the wire, whatever a document may spell it as.
        value = c.value != 0 ? 1 : 0;
        break;
    case ParamSpec::Option:
        value = c.spec.optionsFrom.isEmpty() ? QJsonValue(c.spec.options.value(int(c.value)).value) : QJsonValue(c.ref);
        break;
    case ParamSpec::Other:
        return;
    }
    m_session->setParam(m_chain, c.node, c.spec.id, value);
}

QString NodeEditorModel::paramAt(int index) const
{
    return index >= 0 && index < m_controls.size() ? m_controls.at(index).spec.id : QString();
}

int NodeEditorModel::indexOfParam(const QString &param) const
{
    for (int i = 0; i < m_controls.size(); ++i)
        if (m_controls.at(i).spec.id == param)
            return i;
    return -1;
}

double NodeEditorModel::valueAt(int index) const
{
    return index >= 0 && index < m_controls.size() ? m_controls.at(index).value : 0;
}

double NodeEditorModel::positionOf(const ParamSpec &p, double value)
{
    const double lo = Schema::lowerOf(p), hi = Schema::upperOf(p);
    if (hi <= lo)
        return 0;
    double pos = (p.log && lo > 0 && value > 0) ? std::log(value / lo) / std::log(hi / lo) : (value - lo) / (hi - lo);
    return std::clamp(pos, 0.0, 1.0);
}

double NodeEditorModel::valueAt(const ParamSpec &p, double position)
{
    const double lo = Schema::lowerOf(p), hi = Schema::upperOf(p);
    position = std::clamp(position, 0.0, 1.0);
    double v = (p.log && lo > 0) ? lo * std::pow(hi / lo, position) : lo + position * (hi - lo);
    if (p.step > 0)
        v = lo + std::round((v - lo) / p.step) * p.step;
    return std::clamp(v, lo, hi);
}

QString NodeEditorModel::format(const ParamSpec &p, double v)
{
    // Three significant figures, without trailing zeros: "5 ms", "0.35", "380 ms".
    const auto number = [](double x) {
        return std::abs(x) >= 1000 ? QString::number(x, 'f', 0) : QString::number(x, 'g', 3);
    };
    // The engine's percentages are fractions (a mix of 0–1 with the unit "%").
    if (p.unit == QLatin1String("%") && Schema::upperOf(p) <= 1)
        return QString::number(std::lround(v * 100)) + QStringLiteral(" %");
    if (p.unit == QLatin1String("dB"))
        return (v > 0.005 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', 1) + QStringLiteral(" dB");
    if (p.unit == QLatin1String("Hz")) {
        if (v >= 1000)
            return QString::number(v / 1000, 'g', 3) + QStringLiteral(" kHz");
        return number(v) + QStringLiteral(" Hz");
    }
    const QString n = p.step >= 1 ? QString::number(std::llround(v)) : number(v);
    return p.unit.isEmpty() ? n : n + u' ' + p.unit;
}

QString NodeEditorModel::textOf(const Control &c) const
{
    if (c.kind == QLatin1String("picker")) {
        const auto kind = c.spec.optionsFrom == QLatin1String("irs") ? LibraryEntry::Ir : LibraryEntry::Model;
        const LibraryEntry *entry = m_library ? m_library->find(kind, c.ref) : nullptr;
        return entry ? entry->name : c.ref.isEmpty() ? tr("(none)") : c.ref.section(u'/', -1);
    }
    if (c.spec.kind == ParamSpec::Option)
        return c.spec.options.value(int(c.value)).label;
    if (c.spec.kind == ParamSpec::Flag)
        return c.value != 0 ? tr("on") : tr("off");
    return format(c.spec, c.value);
}

int NodeEditorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_controls.size());
}

QVariant NodeEditorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_controls.size())
        return {};
    const Control &c = m_controls.at(index.row());
    switch (role) {
    case KindRole:
        return c.kind;
    case LabelRole:
        return c.spec.name;
    case GroupRole:
        return c.group;
    case FirstOfGroupRole:
        return c.firstOfGroup;
    case PositionRole:
        if (c.spec.kind == ParamSpec::Option)
            return c.spec.options.size() > 1 ? c.value / (c.spec.options.size() - 1) : 0.0;
        return positionOf(c.spec, c.value);
    case TextRole:
        return textOf(c);
    case OptionsRole: {
        QStringList labels;
        for (const auto &o : c.spec.options)
            labels << o.label;
        return labels;
    }
    case CurrentRole:
        return int(c.value);
    case StreamingRole:
        return streams(c);
    case ParamRole:
        return c.spec.id;
    }
    return {};
}

QHash<int, QByteArray> NodeEditorModel::roleNames() const
{
    return {
        {KindRole, "kind"}, {LabelRole, "label"}, {GroupRole, "group"}, {PositionRole, "position"},
        {TextRole, "valueText"}, {OptionsRole, "options"}, {CurrentRole, "current"},
        {StreamingRole, "streaming"}, {FirstOfGroupRole, "firstOfGroup"}, {ParamRole, "param"},
    };
}

} // namespace rigpa
