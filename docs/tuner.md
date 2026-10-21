# The tuner in rigpa

![The tuner view](screenshots/tuner.png)

The pedal's tuner: a main view of its own, reached from the menu button directly above Settings
(`qml/Menu.qml`). It shows the note, how many cents off it is, the frequency and the input level,
and while it is up **the rig is silent** — the amp behind the tuner hears nothing, so the player
tunes against a still amp rather than a speaker in the room.

The engine half is `nc.tuner` (`../namchain/docs/tuner.md`): squatch-dsp's tuner as a node with one
input, one output and a silent output. This page is the UI's side of it.

## How the view turns into a muted rig

The node is **not** in the rig. It is mounted while the view is up and removed when it goes:

1. The menu button (or `--view tuner`) calls `Navigator::showMainView(TunerView)`.
2. `App` connects `Navigator::mainViewChanged` to two things: `RigSession::setTuner(true)` and
   `TunerModel::setWatching(true)`.
3. `RigSession` publishes the running document **with one `nc.tuner` after each chain's input**
   (`edits::injectTuner`, `src/rig/RigEdits.*`), marked `meta.ephemeral`. That publish is not
   recorded: it is not undoable, it is not drawn on the canvas, and it is not saved.
4. The node's output is silence, so every chain is muted. `TunerModel` polls `rig.state` at 30 Hz
   while the view is up and draws the reading.
5. Back, or another main view, does the same in reverse: `setTuner(false)` publishes the document
   without the node, and the rig is audible again.

Three consequences worth knowing:

- **The node survives the edits you make while the tuner is up.** Any document rigpa publishes while
  the tuner is on carries the node (`RigSession::applyJson` injects on the way out), so an edit made
  with the tuner on screen does not un-mute the rig.
- **The engine's document is stripped before rigpa adopts it.** `rig.get` returns what is running,
  which includes the node; `RigSession::fetchDocument` and `RigManager::saveNow` strip it, so the
  canvas, the undo history and the library never see it. pedald does the same before it writes its
  resume record (`devices/common/rootfs/usr/local/lib/pedald/resume.py`).
- **A rig that arrives from elsewhere gets the node too.** While the view is up, a document adopted
  from the engine (a footswitch through pedald, a `rig.load`) is published again with the node, once
  per generation so a refused mount does not retry for ever. "The tuner is up" means the rig is
  muted, whichever rig it is.

## What the view says

| | |
|---|---|
| the note | `A4`, big, in `Theme.good` when it is in tune and `Theme.text` when it is not |
| the cents | one decimal, signed; the needle on a ±50-cent scale, the in-tune band ±5 cents |
| the frequency | the engine's `hz`, one decimal |
| the level | the engine's `level_db` as a bar over −60…0 dBFS, so "nothing is plugged in" is visible |
| "The rig is muted while this view is open" | shown when the engine's state contains the node |
| "The rig is not muted — the engine is running no tuner node" | in `Theme.warn`, when a publish was refused or an engine too old for the node is running. This is the honest failure mode: the view is up and the amp is still audible, and the player has to know |

The engine withholds a note it does not trust — a decaying string in the room's hum, partials that
disagree by more than 3 cents — and in that case the state omits the note rather than repeating the
last one, so the view shows "No note" and an em dash. A note held perfectly steady does not repaint
the screen: the model compares what it would draw (needle in pixels, texts, in-tune) and emits only
on a change, so `blocks` moving alone is not a redraw.

## Hardware controls

The view has nothing to turn. Its focus ring is empty, so the rotary and apply do nothing there, and
back returns to the rig view — which is also what unmounts the node. `docs/spec.md`'s table is the
specification and `tests/tst_navigator.cpp` the oracle.

A menu button is the activation until the Pico's controls path exists: the pedal's footswitches reach
rigpa through pedald, and mapping one to the tuner is that work, not this view's.

## The model

`src/rig/TunerModel.*` is the reading and nothing else: `rig.state`, one request in flight, 30 Hz,
started and stopped by the view. It exposes what the view draws — `noteText`, `centsText`, `hzText`,
`levelText`, `inTune`, `needleX` (pixels in a `barWidth`-wide scale), `levelFraction`, `status` —
with the layout maths in C++, as everywhere else. `available`, `running` and `valid` are three
different facts and the view uses each: "the engine has answered", "the engine is running the node"
and "the engine has a note".

## What is not here

- A moving strobe. namchain publishes the reading, not the per-partial phases a strobe animates to;
  a strobe wants a phase rate from the engine or a client integrating cents on its own clock.
- A reference-pitch control (A4 is 440 Hz, the engine's).
- Which engine the tuner works against: any that publishes `nc.tuner` in `engine.schema` and the
  `node_state` feature. `EngineSession::hasTuner()` is that check, and the menu button is disabled
  without it rather than publishing a document the engine would refuse.

## Tests

- `tests/tst_tuner.cpp` — the model, from synthetic `rig.state` replies: which node is ours, a
  withheld note shown as none, the needle's ends, the level's fraction, no redraw when only `blocks`
  moves, and everything forgotten when the view is left.
- `tests/tst_tuner_live.cpp` — against a real `namchaind`: mounting puts one node after each chain's
  input (two chains in the Dual fixture), the state reports it, rigpa's own document never holds it,
  and unmounting leaves the document as it was.
- `tests/tst_edits.cpp` — `injectTuner`/`stripTuner` on documents: every chain, a bare-graph
  document, a non-editable chain's splice, id collisions, a hand-written tuner node left alone, and
  `strip(inject(d)) == d` byte for byte.
- `tests/tst_navigator.cpp` — the tuner row of the hardware table.
