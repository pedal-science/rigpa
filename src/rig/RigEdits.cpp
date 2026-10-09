#include "RigEdits.h"

#include <QJsonArray>

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
                const Out end = series(e.lanes.at(l), {e.split.id, l});
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
