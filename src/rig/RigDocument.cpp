#include "RigDocument.h"

#include <QHash>
#include <QJsonArray>
#include <QSet>

#include <algorithm>

namespace rigpa {

namespace {

const QString kInput = QStringLiteral("nc.input");
const QString kOutput = QStringLiteral("nc.output");
const QString kSplit = QStringLiteral("nc.split");
const QString kMix = QStringLiteral("nc.mix");

RigNode nodeFrom(const QJsonObject &o)
{
    RigNode n;
    n.id = o.value(QStringLiteral("id")).toString();
    n.type = o.value(QStringLiteral("type")).toString();
    n.element = o.value(QStringLiteral("element")).toString();
    n.bypass = o.value(QStringLiteral("bypass")).toBool();
    n.json = o;
    return n;
}

struct Edge {
    QString to;
    int fromPort = 0;
};

class Walker
{
public:
    QHash<QString, RigNode> nodes;
    QHash<QString, QList<Edge>> next;
    QSet<QString> visited;
    QString problem;

    // The series from `start` up to (not including) `stop`.
    bool series(const QString &start, const QString &stop, QList<RigElement> &out)
    {
        QString cur = start;
        while (cur != stop) {
            if (!nodes.contains(cur))
                return fail(QStringLiteral("'%1' is not a node").arg(cur));
            if (visited.contains(cur))
                return fail(QStringLiteral("'%1' is reached twice").arg(cur));
            visited.insert(cur);
            const RigNode &node = nodes.value(cur);
            const QList<Edge> &outs = next.value(cur);
            if (node.type == kSplit) {
                RigElement section;
                section.kind = RigElement::Section;
                section.split = node;
                section.element = node.element.isEmpty() ? node.id : node.element;
                const QString mix = findMix(node, outs);
                if (mix.isEmpty())
                    return fail(QStringLiteral("the split '%1' has no mix").arg(cur));
                for (const Edge &e : outs) {
                    QList<RigElement> lane;
                    if (!series(e.to, mix, lane))
                        return false;
                    section.lanes.append(lane);
                }
                section.mix = nodes.value(mix);
                visited.insert(mix);
                const QList<Edge> &after = next.value(mix);
                if (after.size() != 1)
                    return fail(QStringLiteral("the mix '%1' doesn't lead on to one node").arg(mix));
                out.append(section);
                cur = after.first().to;
                continue;
            }
            if (node.type == kOutput || node.type == kInput || node.type == kMix)
                return fail(QStringLiteral("'%1' (%2) is in the middle of the chain").arg(cur, node.type));
            if (outs.size() != 1)
                return fail(QStringLiteral("'%1' leads to %2 nodes").arg(cur).arg(outs.size()));
            const bool joins = !out.isEmpty() && out.last().kind == RigElement::Block
                               && !node.element.isEmpty() && out.last().element == node.element;
            if (joins) {
                out.last().nodes.append(node);
            } else {
                RigElement block;
                block.element = node.element.isEmpty() ? node.id : node.element;
                block.nodes.append(node);
                out.append(block);
            }
            cur = outs.first().to;
        }
        return true;
    }

private:
    bool fail(const QString &why)
    {
        if (problem.isEmpty())
            problem = why;
        return false;
    }

    // The mix that closes a split: the nc.mix sharing its element (ljoss writes them as a pair),
    // else the first nc.mix reached along lane 0.
    QString findMix(const RigNode &split, const QList<Edge> &outs)
    {
        if (!split.element.isEmpty())
            for (auto it = nodes.cbegin(); it != nodes.cend(); ++it)
                if (it->type == kMix && it->element == split.element)
                    return it.key();
        QString cur = outs.isEmpty() ? QString() : outs.first().to;
        QSet<QString> seen;
        while (!cur.isEmpty() && !seen.contains(cur)) {
            seen.insert(cur);
            if (nodes.value(cur).type == kMix)
                return cur;
            const QList<Edge> &o = next.value(cur);
            cur = o.size() == 1 ? o.first().to : QString();
        }
        return {};
    }
};

RigChain parseChain(const QJsonObject &c)
{
    RigChain chain;
    chain.id = c.value(QStringLiteral("id")).toString(QStringLiteral("main"));
    chain.name = c.value(QStringLiteral("name")).toString();
    chain.inputChannel = c.value(QStringLiteral("input_channel")).toInt();
    chain.outputChannel = c.value(QStringLiteral("output_channel")).toInt();

    Walker w;
    QList<RigNode> ordered;
    for (const auto &v : c.value(QStringLiteral("nodes")).toArray()) {
        const RigNode n = nodeFrom(v.toObject());
        w.nodes.insert(n.id, n);
        ordered.append(n);
        if (n.type == kInput)
            chain.input = n;
        else if (n.type == kOutput)
            chain.output = n;
    }
    for (const auto &v : c.value(QStringLiteral("connections")).toArray()) {
        const QJsonObject e = v.toObject();
        w.next[e.value(QStringLiteral("from")).toString()].append(
            {e.value(QStringLiteral("to")).toString(), e.value(QStringLiteral("from_port")).toInt()});
    }
    for (auto &outs : w.next)
        std::stable_sort(outs.begin(), outs.end(), [](const Edge &a, const Edge &b) { return a.fromPort < b.fromPort; });

    bool ok = !chain.input.id.isEmpty() && !chain.output.id.isEmpty();
    if (!ok)
        w.problem = QStringLiteral("the chain has no input or no output");
    if (ok) {
        w.visited.insert(chain.input.id);
        const QList<Edge> &first = w.next.value(chain.input.id);
        if (first.size() != 1) {
            ok = false;
            w.problem = QStringLiteral("the input leads to %1 nodes").arg(first.size());
        } else {
            ok = w.series(first.first().to, chain.output.id, chain.series);
        }
    }
    if (ok && w.visited.size() + 1 != w.nodes.size())
        ok = false, w.problem = QStringLiteral("some nodes are not on the path from input to output");
    if (!ok) {
        // Shown as it is written, one tile per node, and not editable.
        chain.editable = false;
        chain.problem = w.problem;
        chain.series.clear();
        for (const RigNode &n : ordered) {
            if (n.type == kInput || n.type == kOutput)
                continue;
            RigElement block;
            block.element = n.element.isEmpty() ? n.id : n.element;
            block.nodes.append(n);
            chain.series.append(block);
        }
    }
    return chain;
}

} // namespace

RigDocument RigDocument::parse(const QJsonObject &document)
{
    RigDocument doc;
    doc.json = document;
    doc.name = document.value(QStringLiteral("name")).toString();
    if (document.contains(QStringLiteral("chains"))) {
        for (const auto &c : document.value(QStringLiteral("chains")).toArray())
            doc.chains.append(parseChain(c.toObject()));
    } else if (document.contains(QStringLiteral("nodes"))) {
        doc.chains.append(parseChain(document)); // a bare graph is one chain, "main"
    }
    return doc;
}

void RigDocument::flatten(const QList<RigElement> &series, QList<const RigElement *> &out)
{
    for (const RigElement &e : series) {
        out.append(&e);
        if (e.kind == RigElement::Section)
            for (const auto &lane : e.lanes)
                flatten(lane, out);
    }
}

} // namespace rigpa
