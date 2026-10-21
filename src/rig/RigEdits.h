#pragma once

#include "RigDocument.h"

#include <QJsonObject>
#include <QStringList>

namespace rigpa {

// Structural edits on the editor's tree of a document, and the tree written back out as an engine
// document (ljoss's ChainEdits). An edit changes the tree; `toJson` turns it into the nodes and
// connections `rig.apply` takes, keeping every node object, its parameters and `meta` as they were,
// and every key of the document and its chains that the tree doesn't own.
namespace edits {

// Where in a document a series is: a chain, and the lanes of the sections to go down through, by
// the section's element and the lane's index ("<element>/<lane>"). An empty path is the chain's
// own series.
struct Place {
    int chain = 0;
    QStringList path;
    int index = 0;   // the slot in that series, counted without the element being moved

    bool operator==(const Place &o) const { return chain == o.chain && path == o.path && index == o.index; }
    bool operator!=(const Place &o) const { return !(*this == o); }
};

// The series a place names, or null when it doesn't exist (or the chain isn't editable).
QList<RigElement> *series(RigDocument &doc, int chain, const QStringList &path);

// Where an element is now, or {chain -1} when it isn't anywhere editable.
Place locate(const RigDocument &doc, const QString &element);

// Takes an element out of its series. False when it isn't in an editable chain.
bool take(RigDocument &doc, const QString &element, RigElement *taken = nullptr);

// Moves an element to `to` (whose index counts the series without it). Refuses a move into the
// element's own lanes, and a move between chains when either isn't editable.
bool move(RigDocument &doc, const QString &element, const Place &to);

bool remove(RigDocument &doc, const QString &element);

// Inserts an element at a place.
bool insert(RigDocument &doc, const RigElement &element, const Place &at);

// A section's lanes: new ones are added empty at the end; fewer takes lanes off the end, with
// whatever is in them. The split's `outputs` and the mix's `inputs` follow.
bool setLaneCount(RigDocument &doc, const QString &section, int lanes);

// A lane's level, mute and polarity (ljoss's lane gain, RigElement::laneGains). At 0 dB, unmuted
// and upright the lane has no gain node at all.
struct LaneGain {
    double levelDb = 0;
    bool mute = false;
    bool invert = false;
    double linear() const;
};
LaneGain laneGainOf(const RigElement &section, int lane);
bool setLaneGain(RigDocument &doc, const QString &section, int lane, const LaneGain &gain);

// The element (a block or section) with this id, to change in place.
RigElement *find(RigDocument &doc, const QString &element);

// New things, with ids as ljoss makes them: a UUID element, and nodes "<element>-<suffix>".
QString newElementId();
RigNode newNode(const QString &element, const QString &suffix, const QString &type,
                const QJsonObject &params = {});
RigElement newSection(int lanes = 2);

// The document the tree says, for rig.apply.
QJsonObject toJson(const RigDocument &doc);

// --- The tuner's ephemeral node (namchain docs/tuner.md) --------------------
//
// A tuner is a UI element, not a block: while it is on screen the document that is *published* has
// one `nc.tuner` after each chain's input, marked `meta.ephemeral`, and the rig is silent because the
// node's output is silence. The editor's own document never holds it -- that is what makes it
// invisible, un-undoable and unsaveable.
//
// These work on the document JSON rather than on the tree, so they also reach a chain the editor
// calls un-editable (a hand-written fan-out) and the bare-graph form of a one-chain document.

// Splices one tuner in after each chain's input, and repoints at it everything the input fed.
// Returns how many chains gained one; 0 when the document already has them or has no input node.
/// \param muted whether the node's output should be silence (its default) or its input.
///
/// The switch is a parameter of the node, so it is part of the document: mounting while the player
/// has told the tuner to pass the signal through has to say so here, or the first publish after a
/// toggle would mute the rig again (namchain docs/tuner.md, "The switch").
int injectTuner(QJsonObject &document, bool muted = true);

// The exact inverse: each consumer of a tuner's output is repointed at the tuner's predecessor, and
// the tuner goes with its wires. Returns how many chains lost one.
int stripTuner(QJsonObject &document);

// Whether a document carries an injected tuner -- which, on a document read back from the engine,
// is how rigpa tells that the node it mounted is still running.
bool hasTuner(const QJsonObject &document);

} // namespace edits
} // namespace rigpa
