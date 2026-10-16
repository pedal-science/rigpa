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
  view.

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
- All of it is built generically from `engine.schema`.
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

Any touch moves the hardware highlight to what was touched. Footswitches and MIDI keep their
existing path (Pico → pedald → `rig.load`), and rigpa follows. On the pedal, the rotary, apply and
back are wired to the **Pico**: new UART event kinds, forwarded by pedald over a local socket.
That path is specified now and built after the Mac proxies.

**On the Mac**:
- `rigpa.json` sets the engine and pedald addresses, the token file, the library roots and a
  window scale (1920×480 doesn't fit a laptop at 1×): the size the window opens at. It can be
  resized or made full screen (the green button, e.g. on the pedal's own screen over HDMI), and the
  UI scales to fit, letterboxed.
- Trackpad scroll stands in for touch scrolling.
- **Cmd + trackpad scroll = main rotary.** Pixel deltas are gathered into detents, and Qt reports
  Cmd as `ControlModifier`.
- `.` = apply, `,` = back. Neither fires while a text field has focus.
- ⌘Z and ⇧⌘Z undo and redo.

**Later, not now**: the Pico controls path, ljoss's "my document wins" on connect (R6, it overwrites
the pedal's rig), dropping TCP/auth, banks and MIDI‑map UI, NAM/IR uploads.
