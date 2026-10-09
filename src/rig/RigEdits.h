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

} // namespace edits
} // namespace rigpa
