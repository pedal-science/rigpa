#include "RigEdits.h"

#include <QJsonArray>
#include <QUuid>

#include <cmath>

namespace rigpa::edits {

namespace {

bool findIn(const QList<RigElement> &series, const QString &element, QStringList &path, int &index)
{
    for (int i = 0; i < series.size(); ++i) {
        const RigElement &e = series.at(i);
        if (e.element == element) {
            index = i;
            return true;
        }
        for (int l = 0; l < e.lanes.size(); ++l) {
            path.append(e.element + u'/' + QString::number(l));
            if (findIn(e.lanes.at(l), element, path, index))
                return true;
            path.removeLast();
        }
    }
    return false;
}

struct Out {
    QString node;
    int port = 0;
};

class Writer
{
public:
    QJsonArray nodes;
    QJsonArray connections;

    void connect(const Out &from, const QString &to, int toPort)
    {
        connections.append(QJsonObject{{QStringLiteral("from"), from.node},
                                       {QStringLiteral("from_port"), from.port},
                                       {QStringLiteral("to"), to},
                                       {QStringLiteral("to_port"), toPort}});
    }

    Out series(const QList<RigElement> &list, Out from)
    {
        for (const RigElement &e : list) {
            if (e.kind == RigElement::Block) {
                for (const RigNode &n : e.nodes) {
                    nodes.append(n.json);
                    connect(from, n.id, 0);
                    from = {n.id, 0};
                }
                continue;
            }
            nodes.append(e.split.json);
            connect(from, e.split.id, 0);
            for (int l = 0; l < e.lanes.size(); ++l) {
                Out end = series(e.lanes.at(l), {e.split.id, l});
                const RigNode gain = e.laneGains.value(l);
                if (!gain.id.isEmpty()) {
                    nodes.append(gain.json);
                    connect(end, gain.id, 0);
                    end = {gain.id, 0};
                }
                connect(end, e.mix.id, l);
            }
            nodes.append(e.mix.json);
            from = {e.mix.id, 0};
        }
        return from;
    }
};

QJsonObject chainJson(const RigChain &chain)
{
    if (!chain.editable)
        return chain.json;
    Writer w;
    w.nodes.append(chain.input.json);
    const Out end = w.series(chain.series, {chain.input.id, 0});
    w.nodes.append(chain.output.json);
    w.connect(end, chain.output.id, 0);
    QJsonObject json = chain.json;
    json.insert(QStringLiteral("nodes"), w.nodes);
    json.insert(QStringLiteral("connections"), w.connections);
    return json;
}

} // namespace

QList<RigElement> *series(RigDocument &doc, int chain, const QStringList &path)
{
    if (chain < 0 || chain >= doc.chains.size() || !doc.chains.at(chain).editable)
        return nullptr;
    QList<RigElement> *list = &doc.chains[chain].series;
    for (const QString &step : path) {
        const QString element = step.section(u'/', 0, -2);
        bool ok = false;
        const int lane = step.section(u'/', -1).toInt(&ok);
        QList<RigElement> *next = nullptr;
        for (RigElement &e : *list)
            if (e.kind == RigElement::Section && e.element == element && ok && lane >= 0 && lane < e.lanes.size())
                next = &e.lanes[lane];
        if (!next)
            return nullptr;
        list = next;
    }
    return list;
}

Place locate(const RigDocument &doc, const QString &element)
{
    for (int c = 0; c < doc.chains.size(); ++c) {
        if (!doc.chains.at(c).editable)
            continue;
        Place p;
        p.chain = c;
        if (findIn(doc.chains.at(c).series, element, p.path, p.index))
            return p;
    }
    return {-1, {}, 0};
}

bool take(RigDocument &doc, const QString &element, RigElement *taken)
{
    const Place at = locate(doc, element);
    QList<RigElement> *list = at.chain >= 0 ? series(doc, at.chain, at.path) : nullptr;
    if (!list)
        return false;
    if (taken)
        *taken = list->at(at.index);
    list->removeAt(at.index);
    return true;
}

bool insert(RigDocument &doc, const RigElement &element, const Place &at)
{
    QList<RigElement> *list = series(doc, at.chain, at.path);
    if (!list || at.index < 0 || at.index > list->size())
        return false;
    list->insert(at.index, element);
    return true;
}

bool move(RigDocument &doc, const QString &element, const Place &to)
{
    for (const QString &step : to.path)
        if (step.section(u'/', 0, -2) == element)
            return false; // into its own lanes
    RigDocument copy = doc;
    RigElement taken;
    if (!take(copy, element, &taken) || !insert(copy, taken, to))
        return false;
    doc = copy;
    return true;
}

bool remove(RigDocument &doc, const QString &element)
{
    return take(doc, element);
}

namespace {

RigElement *findIn(QList<RigElement> &series, const QString &element)
{
    for (RigElement &e : series) {
        if (e.element == element)
            return &e;
        for (auto &lane : e.lanes)
            if (RigElement *found = findIn(lane, element))
                return found;
    }
    return nullptr;
}

void setParam(RigNode &node, const QString &param, const QJsonValue &value)
{
    node.json.insert(param, value);
}

} // namespace

RigElement *find(RigDocument &doc, const QString &element)
{
    for (RigChain &chain : doc.chains)
        if (chain.editable)
            if (RigElement *found = findIn(chain.series, element))
                return found;
    return nullptr;
}

bool setLaneCount(RigDocument &doc, const QString &section, int lanes)
{
    RigElement *e = find(doc, section);
    if (!e || e->kind != RigElement::Section || lanes < 1)
        return false;
    while (e->laneGains.size() < e->lanes.size())
        e->laneGains.append(RigNode{});
    while (e->lanes.size() < lanes) {
        e->lanes.append(QList<RigElement>());
        e->laneGains.append(RigNode{});
    }
    while (e->lanes.size() > lanes) {
        e->lanes.removeLast();
        e->laneGains.removeLast();
    }
    setParam(e->split, QStringLiteral("outputs"), lanes);
    setParam(e->mix, QStringLiteral("inputs"), lanes);
    return true;
}

double LaneGain::linear() const
{
    const double magnitude = mute ? 0 : std::pow(10.0, levelDb / 20);
    return invert ? -magnitude : magnitude;
}

LaneGain laneGainOf(const RigElement &section, int lane)
{
    LaneGain g;
    const RigNode node = section.laneGains.value(lane);
    if (node.id.isEmpty())
        return g;
    const double linear = node.json.value(QStringLiteral("gain")).toDouble(1);
    // A muted lane remembers its level and polarity in the node's meta: a gain of 0 can't say them.
    const QJsonObject meta = node.json.value(QStringLiteral("meta")).toObject();
    g.mute = linear == 0;
    g.invert = g.mute ? meta.value(QStringLiteral("invert")).toBool() : linear < 0;
    g.levelDb = g.mute ? meta.value(QStringLiteral("level_db")).toDouble(0) : 20 * std::log10(std::abs(linear));
    return g;
}

bool setLaneGain(RigDocument &doc, const QString &section, int lane, const LaneGain &gain)
{
    RigElement *e = find(doc, section);
    if (!e || e->kind != RigElement::Section || lane < 0 || lane >= e->lanes.size())
        return false;
    while (e->laneGains.size() < e->lanes.size())
        e->laneGains.append(RigNode{});
    RigNode &node = e->laneGains[lane];
    if (std::abs(gain.levelDb) < 1e-9 && !gain.mute && !gain.invert) {
        node = {};
        return true;
    }
    if (node.id.isEmpty()) {
        const QString element = newElementId();
        node = newNode(element, QStringLiteral("gain"), QStringLiteral("nc.gain"));
    }
    setParam(node, QStringLiteral("gain"), gain.mute ? 0.0 : gain.linear());
    QJsonObject meta = node.json.value(QStringLiteral("meta")).toObject();
    meta.remove(QStringLiteral("level_db"));
    meta.remove(QStringLiteral("invert"));
    if (gain.mute) {
        meta.insert(QStringLiteral("level_db"), gain.levelDb);
        if (gain.invert)
            meta.insert(QStringLiteral("invert"), true);
    }
    if (meta.isEmpty())
        node.json.remove(QStringLiteral("meta"));
    else
        node.json.insert(QStringLiteral("meta"), meta);
    return true;
}

QString newElementId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

RigNode newNode(const QString &element, const QString &suffix, const QString &type, const QJsonObject &params)
{
    RigNode n;
    n.id = element + u'-' + suffix;
    n.type = type;
    n.element = element;
    QJsonObject json = params;
    json.insert(QStringLiteral("id"), n.id);
    json.insert(QStringLiteral("type"), type);
    json.insert(QStringLiteral("element"), element);
    n.json = json;
    return n;
}

RigElement newSection(int lanes)
{
    RigElement s;
    s.kind = RigElement::Section;
    s.element = newElementId();
    s.split = newNode(s.element, QStringLiteral("split"), QStringLiteral("nc.split"), {{QStringLiteral("outputs"), lanes}});
    s.mix = newNode(s.element, QStringLiteral("mix"), QStringLiteral("nc.mix"), {{QStringLiteral("inputs"), lanes}});
    for (int l = 0; l < lanes; ++l) {
        s.lanes.append(QList<RigElement>());
        s.laneGains.append(RigNode{});
    }
    return s;
}

QJsonObject toJson(const RigDocument &doc)
{
    QJsonObject json = doc.json;
    if (!json.contains(QStringLiteral("chains")) && doc.chains.size() == 1) {
        // A bare graph stays one: its nodes and connections at the top.
        const QJsonObject chain = chainJson(doc.chains.first());
        json.insert(QStringLiteral("nodes"), chain.value(QStringLiteral("nodes")));
        json.insert(QStringLiteral("connections"), chain.value(QStringLiteral("connections")));
        return json;
    }
    QJsonArray chains;
    for (const RigChain &c : doc.chains)
        chains.append(chainJson(c));
    json.insert(QStringLiteral("chains"), chains);
    return json;
}

} // namespace rigpa::edits
