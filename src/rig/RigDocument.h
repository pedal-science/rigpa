#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace rigpa {

// One node of an engine document, as written: everything but id, type, element and bypass is a
// parameter (namchain docs/rig-pipeline.md).
struct RigNode {
    QString id;
    QString type;
    QString element;
    bool bypass = false;
    QJsonObject json;
};

// What the editor shows of a chain: a series of elements from Input to Output.
//   A block is consecutive nodes that share an `element` (ljoss's NAM + tone stack is one block).
//   A section is an nc.split … nc.mix pair; each lane between them is a series of its own.
struct RigElement {
    enum Kind { Block, Section };
    Kind kind = Block;
    QString element;              // the element id (a lone node without one: its node id)
    QList<RigNode> nodes;         // Block: in signal order
    RigNode split, mix;           // Section
    QList<QList<RigElement>> lanes;
};

struct RigChain {
    QString id;
    QString name;
    int inputChannel = 0;
    int outputChannel = 0;
    RigNode input, output;
    QList<RigElement> series;
    // False when the graph isn't a series of blocks and split/mix sections (hand-written fan-out,
    // a cycle, a dangling node): then `series` is every node in document order, shown as is, and
    // `problem` says why it can't be edited.
    bool editable = true;
    QString problem;
    QJsonObject json;             // the chain as received: its other keys survive an edit
};

struct RigDocument {
    QString name;
    QList<RigChain> chains;
    QJsonObject json;            // the document as received

    static RigDocument parse(const QJsonObject &document);
    // Every block and section, depth first: the order the rotary moves through.
    static void flatten(const QList<RigElement> &series, QList<const RigElement *> &out);
};

} // namespace rigpa
