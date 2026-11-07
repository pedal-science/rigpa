# Rigpa: specification

The owner's requirements (2026-10-09), organised, with the answers to the questions they raised.
The plan that builds this is [plan.md](plan.md).

**What rigpa is.** The pedal's own UI, on the Core host's one housekeeping core, 0 (`CPUAffinity=0`).
Since 2026-10-09 the pedal isolates cores 1–3 for rendering and the audio DMA interrupt is delivered
to core 1 — a render core, never the active one — so it no longer preempts rigpa on core 0
(`pedal.project/docs/managing-processing-power-and-optimising-for-fast-switching.md`, the rules and
D5). SCHED_OTHER, never real‑time. Fullscreen on the 1920×480 touchscreen, long side
horizontal. C++ and Qt 6.10, **Qt Quick for the views only** (QML for layout, gestures,
animation); protocol, models, layout maths, navigation and settings are C++. Platform `eglfs`
(eglfs_kms) on vip, `cocoa` on the Mac, `offscreen`/`vnc` for tests and for viewing before the
screen arrives.

**Data.**
- `namchaind`: TCP 127.0.0.1:47810, protocol 2, the existing token (on vip
  `$CREDENTIALS_DIRECTORY/token` via `LoadCredential`; else a file named in config). Later work may
  drop TCP or auth.
- NAM and IR files: read directly, read‑only, from the roots namchaind uses; rigpa parses their
  metadata itself (namchaind doesn't).
- Rigs: through pedald's HTTP API (47820, same token as bearer), which owns the rig library:
  list, get, save, new, rename, duplicate, delete.
- On vip the facts come from `/etc/namchain/namchaind.json` and `/etc/pedal/pedald.json`, read
  the way pedald reads them (R12, one home per fact). `rigpa.json` holds only rigpa's own
  settings, plus explicit overrides for the Mac.

**Layout** (pixels, 1920×480):
```
| menu 96 |  header 64                                       | info 320 |
|         |  content 368 (the active main view)              |  (one    |
|         |  footer 48                                       |  widget) |
```
- **menu**: five 96×96 buttons.
  - Slot 1: ljoss's "pedals on a rail" icon, which goes to rig view, and is **blank while rig
    view is the active main view**.
  - Slot 2: folder icon, which opens browse view.
  - Slots 3–4: empty.
  - Slot 5: settings icon, which opens settings view.
- **header**: the view's title and the running rig's name with a saved/unsaved mark. In rig view
  it also has Add (+), Undo and Redo.
- **footer**: what apply and back do right now, plus transient messages (connection errors,
  refusals).
- **info** (full height):
  - connection state;
  - engine health, DSP load (`render.worst_percent`) and xruns;
  - input and output level meters, polled about 10 Hz and switchable off in settings, so the
    screen can be fully static.
- A **main view** is anything that fills content: rig view (the default), browse view, settings
  view, and the **tuner view** (note, cents, a 1x/2x/4x strobe, and the switch that opens the tuner's
  mute). A main view may own *engine state* as well as pixels: the tuner is
  the first, and while it is up the document the engine runs has a tuner node after each chain's
  input, so the rig is silent (docs/tuner.md).

**Rig view** = `| rig list | rig editor |`.
- The rig list width is a setting (192–576 px, step 16, default 288), changed only in settings
  view.
- **Rig list**: library rigs by name, with the running rig marked.
  - Rig management uses a row of touch buttons under the list: new, rename, duplicate, delete
    (delete asks first).
  - Names are typed on Qt Virtual Keyboard, which is GPLv3/commercial; that matters only if
    binaries are ever distributed.
- **Rig editor**:
  - Each chain is a row of node tiles from Input to Output that wraps onto further rows. Parallel
    sections are drawn with their lanes.
  - One node is **selected**, with a subtle highlight.
  - Kinetic scrolling.
  - Add (+) opens a palette of engine node types, including "parallel split". The new node goes
    after the selected one.
  - A node is removed from its editor, or by dragging it to a trash zone.
  - Undo and redo.
- **Touch**:
  - Tap selects a node. Tapping the node that is already selected opens its editor once the
    double‑tap window (~300 ms) passes. **Double‑tap toggles bypass.**
  - Long‑press lifts a node. On the Mac, click‑and‑drag does.
- **Drag and drop** is dynamic, as in ljoss:
  - The other nodes move aside to show the drop slot, and rows re‑wrap live.
  - A node can be dropped into or out of parallel lanes, or onto trash.
  - The view auto‑scrolls near the edges.
- **Startup**: show the rig namchaind is running. If it runs none, `rig.load` the first rig in the
  list.
- rigpa follows rig changes made by others (footswitch, MIDI, ljoss) by polling.

**Node editor** covers the whole content area. The header shows the node's name.
- Title row: `[Remove] … [Bypass switch][Close]`. The bypass switch is to the left of Close.
- Every control fits without scrolling: rotary knobs for numbers, toggles for flags, and
  segmented choices for small option sets.
- A **model/IR picker** shows a library list with metadata. It is rotary‑friendly and commits on
  apply or tap.
- A **flowEQ block** (`nc.eq.flow`) gets its own layout rather than the generic list: its eight lite
  controls, and the response curve the engine computed for it (`rig.state`). See
  [floweq.md](floweq.md).
- Everything else is built generically from `engine.schema`.
- Knobs on parameters the engine ramps stream `rig.patch set_param` at most every 10 ms, with one
  patch in flight and the last value always sent. Other parameters commit on release.

**Browse view**:
- A Models | IRs switch.
- A list grouped by folder. Each row shows the name and make·model·rate·frames·size, as in ljoss.
- No storage‑location settings.

**Settings view**: rig list width, live meters on/off.

**Saving**:
- Auto‑save about 1 s after the last change, `PUT` through pedald, to the library rig the running
  rig came from. That rig is matched by path, or by document name after a reboot has resumed
  `last.json`.
- A running rig that isn't in the library (for example one ljoss applied) is not auto‑saved. It is
  marked unsaved, and "Save as…" in the rig list stores it under a new name.

**Hardware control model** (main rotary, apply, back; in rig view the levels are rig list → rig
editor → node editor):

| Where | rotary | apply | back |
|---|---|---|---|
| Rig editor (default) | move the selected node along the chain, then on into the next chain | open node editor | go up to rig list |
| Rig list | move the highlight through rigs | load that rig, return to its editor | nothing |
| Node editor / settings: moving the highlight | move highlight (reading order, then Bypass, Close) | non‑boolean → **selected** (editing); boolean → toggle; Close → close; picker → open | node editor: close; settings: go to rig view |
| …a control **selected** | change the value (immediately applied) | deselect | deselect |
| Picker / palette | move through the list | choose and close | close |
| Browse (nothing deeper) | move up and down the list; the Models/IRs switch is the first stop | on the switch: toggle; on an item: nothing yet | go to rig view |
| Tuner | one stop (the mute switch); the rotary does not wrap, so it stays there | on the switch: mute or unmute | go to rig view, which also removes the tuner node and unmutes the rig |

Any touch moves the hardware highlight to what was touched. Footswitches and MIDI keep their
existing path (Pico → pedald → `rig.load`), and rigpa follows. On the pedal, the rotary, apply and
back are wired to the **Pico**: new UART event kinds, forwarded by pedald over a local socket.
That path is specified now and built after the Mac proxies.

**The compact layout** (the box's 4.3″ panel, 800×480 landscape; owner's requirements 2026-11-07,
pedal.project D17 and `docs/pedal-owns-state.md`, milestones C1–C3):
- Chosen at startup: `display.layout` in `rigpa.json` is `wide` (the default, everything above) or
  `compact`; `--layout` overrides it, and an unknown name is refused (exit 2). Every size that
  differs is in `LayoutSpec`; the models are the same `rigpa_core`, and only the views differ.
  The panel is as dense as the bar (about 220 pixels to the inch), so the type is the bar's.
- A top bar (52), one view at a time (800×388), and a bottom bar (40) with what apply and back do.
  No menu and no info column: the engine's health and DSP load sit at the top right, and its status
  and meters are in Settings.
- **Play** is home: the running rig's name in 64 px type, readable from standing, then "Bank 1 ·
  3 of 8 · Next: Crunch" (pedald's `/status`, polled once a second until pedald pushes it), the last
  change, the rig's blocks as chips in signal order (a tap switches one on or off), and Rigs, Edit,
  Tuner and Settings.
- **Rigs**: the rig list full screen, with New, Rename (or Save as…), Copy and Delete under it.
  Choosing a rig loads it and goes home to Play.
- **Edit**: the chain editor, with the bar's touch rules at the panel's tile size (132×72), and +,
  Undo and Redo in the top bar. **Tuner** and **Settings** as on the bar, arranged for the panel.
- The overlays (node editor, picker, palette, rig dialog) are the bar's, filling the panel's
  800×388; flowEQ's panel narrows its curve to 280 px and puts its eight controls four to a row
  beside it. The rig dialog's name row sits above the 800 px keyboard.
- Every view is one step from Play: back, or "‹ Play" in the top bar.

| Where (compact) | rotary | apply | back |
|---|---|---|---|
| Play | the chips, then Rigs, Edit, Tuner, Settings | chip: switch it; button: open its view | nothing (Play is the top) |
| Rigs | the rigs, then New, Rename, Copy, Delete | rig: load it and go to Play; button: its dialog | go to Play |
| Edit | the blocks in signal order | open the node editor | go to Play |
| Node editor, picker, palette, dialog | as on the bar | as on the bar | as on the bar |
| Tuner, Settings | as on the bar | as on the bar | go to Play |

**On the Mac**:
- `rigpa.json` sets the engine and pedald addresses, the token file, the library roots and a
  window scale (1920×480 doesn't fit a laptop at 1×): the size the window opens at. It can be
  resized or made full screen (the green button, e.g. on the pedal's own screen over HDMI), and the
  UI scales to fit, letterboxed.
- `--layout compact` (or `display.layout`) shows the panel's views; with `display.scale` 1 the
  window is exactly 800×480, and `--view play|rigs|edit|tuner|settings` starts in one of them.
- Trackpad scroll stands in for touch scrolling.
- **Cmd + trackpad scroll = main rotary.** Pixel deltas are gathered into detents, and Qt reports
  Cmd as `ControlModifier`.
- `.` = apply, `,` = back. Neither fires while a text field has focus.
- ⌘Z and ⇧⌘Z undo and redo.

**Later, not now**: the Pico controls path, ljoss's "my document wins" on connect (R6, it overwrites
the pedal's rig), dropping TCP/auth, banks and MIDI‑map UI, NAM/IR uploads.
