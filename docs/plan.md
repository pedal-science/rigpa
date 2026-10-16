# Rigpa: plan

How [spec.md](spec.md) gets built. Agreed 2026-10-09.

## Next session starts with

- **V3 started (2026-10-16): the screen is on vip.** What it found:
  - **Mode.** The panel is on HDMI-A-2 (vc4, card1) and its EDID prefers **480×1920**, portrait
    (also 504×315 and 328×205). eglfs takes the preferred mode, so `kms.json` names no mode.
  - **Rotation 270.** With the panel landscape as it sits in the pedal, 90 is upside down.
    pedal.project's `etc/rigpa/rigpa.json` now says 270.
  - **Touch needs no mapping.** The digitiser (`wch.cn USB2IIC_CTP_CONTROL`, 0–4096 on both axes,
    through libinput) runs along the native portrait scan: a left→right drag in landscape is raw Y
    4044→34, top→bottom is raw X 264→3959, and the corner taps agree. Through rotation 270 that is
    the identity, so QML's transform maps it with no calibration matrix.
  - **A screen that comes up after boot.** The unit's `ExecCondition` ran at 14 s, before the panel
    had connected, and skipped; a screen plugged in later was never seen. pedal.project's
    `80-rigpa-screen.rules` starts rigpa on DRM's hotplug event: an unplug and replug brought it
    back, upright, with no hand on vip.
  - **SIGTERM is a clean exit (fixed on the Mac, not yet on vip).** Every `systemctl stop rigpa`
    logged status 1 because eglfs's `QFbVtHandler` catches TERM and INT and calls `_exit(1)` (qtbase
    6.10, `qfbvthandler.cpp`); on the Mac a TERM killed it (143). `QuitOnSignal`, made just after the
    `QGuiApplication` so its handler replaces eglfs's, turns both into `quit()`: exit 0, destructors
    run. `tst_quitonsignal` sends the real app TERM. vip gets it with the next rigpa it is built with.
  - **Taps land where pressed** on rigpa's own controls at 270 (the owner, on the screen).
  - **No brightness control in software.** Nothing answers DDC/CI (0x37) on the panel's DDC bus
    (i2c-14), where its EDID (0x50) does, and its USB is one HID interface, the touch. Stopping the
    signal with the cable in (fbcon powerdown blank: the CRTC inactive, DPMS Off, still `connected`)
    leaves a **lit black screen** for minutes; only pulling the HDMI cable darkens it. Presumably the panel
    sleeps on losing the source's +5 V (HDMI pin 18), which the Pi does not switch (not traced). A screen-off timeout in
    rigpa would save nothing. Dimming means the panel board's backlight enable or PWM pin, from the
    Pico (D2) and well above audio, or rigpa darkening its own pixels (no power saved).
  - **The panel is Waveshare's 8.8inch Side Monitor or a sibling** (its wiki, the owner's match;
    theirs has no touch, a kickstand and side brightness buttons, this one none of the three). Their
    figures: **260 mA dimmest, 500 mA brightest at 5 V**; brightness only by the buttons, so a
    board with unfitted button pads is the cheapest dimming mod (the Pico "pressing" them, no PWM
    near the audio); and some hosts need HDMI before the screen's power or it stays black, cured
    by re-plugging the power — the first suspect if the screen is ever black after a boot. Their
    `hdmi_timings` lines are for the firmware display driver; KMS takes the mode from the EDID.
  - **Not yet:** the screen's USB draw, measured with an inline USB meter that passes data (the
    touch shares the cable), at boot, idle and full brightness, against Waveshare's 500 mA plus the
    touch controller's declared 64 mA. The Pi gives USB 1.6 A only on a
    supply offering 5 V 5 A (the decided Pi 45 W one); on a 5 V 3 A one, as vip was on for this,
    it is 600 mA (`usb_max_current_enable=0`). Also a `systemctl stop rigpa` on vip that logs a clean
    stop.
- **V2 done (2026-10-10): the D14 gate passes.** `MemoryMax` (256 MB, from V2's 141–143 MB PSS) is
  in pedal.project's drop-in. A root that carries numaemu3's `device.env` is owed (pedal.project plan).
- **V2: what it measured.** rigpa as its real unit on eglfs on vc4, HDMI forced on at 1920×480 with a
  generated EDID (no screen yet), beside a 0.553 rig with pedald's warm document; five ten-minute arms
  — off, idle, live meters, `--demo-animate`, off again — each 0 xruns, 0 overruns, worst block ≤ 58%.
  Render p99 23% → 25% → 28% → 31%; rigpa 0.4% / 3% / 18.6% of core 0. Evidence and harness:
  pedal.project `bench-results/2026-10-10-rigpa-soak-vip`.
  - `--demo-animate` (`DemoAnimator`): each node lifted and swept along the editor and back, never
    dropped (`tst_demo_live` checks the engine's generation and document don't move).
  - What V2 needed on vip: the overlay through `vip-boot try-config`; numaemu3 (the HDMI DDC driver,
    backlight and CEC kept through the trim, or vc4 never binds); the unit starting only with a screen
    *connected* (`ExecCondition`), driving vc4 by path (`/etc/rigpa/kms.json`: card0 on the Pi 5 is
    v3d, render-only), and giving up after five failed starts.
  - `mise run vip-vnc` uses macOS Screen Sharing (sharp; TigerVNC isn't Retina-aware, RealVNC wants an
    account), through `dev/vnc-relay.py`.
- **V1: what is on vip.**
  - Kernel: rtpi's numaemu2 (`-numaemu2-trimmed`), numaemu1 plus vc4, v3d and USB HID kept through
    the trim (rtpi `config/vip-keep.fragment`). Promoted through `vip-boot try-kernel`, after the stock
    kernel was tried back into slot b: slot a pedal, slot b fallback, both good.
  - The root (`make-card --remote`, image `20261010T134132Z`): Qt 6.10.2 from the snapshot (eglfs,
    vnc, offscreen, the QML modules), Mesa, a font; rigpa in `/opt/rigpa/<commit>` with its
    `MANIFEST`, `packaging/systemd/rigpa.service` (this repo) plus pedal.project's drop-in (core 0,
    `Nice=5`, idle IO, `OOMScoreAdjust=500`), the `rigpa` user, `/etc/rigpa/rigpa.json`; namchaind
    takes 8 TCP clients. make-card pins rigpa as its fourth repository, builds it in the container
    (`devices/common/image/build-rigpa`, its live tests against the namchaind just built), and renders
    a frame offscreen inside the root, so a missing QML module stops the build.
  - `scripts/deploy-rigpa` (pedal.project) updates it on a running vip; `components.lock` and
    `scripts/status` know it.
  - The unit is enabled and **waits for vc4's HDMI connector**: no screen, no start, no restart loop.
    Run as its user offscreen on vip, it shows vip's rig and status (`build/review/v1-on-vip.png`).
    Three runs added no xruns.
  - **The KMS overlay is open.** vip-boot writes the pedal boot `config.txt` once, at card build, and
    `--remote` replaces only the root, so `dtoverlay=vc4-kms-v3d` can't reach a running vip that way:
    it needs a card write or a vip-boot change (rtpi) that tries a config line as it tries a kernel.
    The plan's V1 step assumed `--remote` would carry it. Decide by V3.
  - To see it before the screen: on vip, `QT_QPA_PLATFORM=vnc:size=1920x480` (unauthenticated, on the
    LAN: start it by hand, stop it after) or `offscreen` with `--screenshot`.
  - Linux fix found on the way: the engine's `/etc/namchain/namchaind.json` is the default only where
    it exists, as pedald's already was.
- **M9: the Mac build against vip.** `mise run vip` (`dev/use-vip`) fetches vip's engine token
  once over ssh (`sudo -n cat /etc/namchain/token`, into `.vip/token`, 0600, never printed) and
  writes `.vip/rigpa.json`; `mise run run -- --config .vip/rigpa.json`. It asks vip for its IPv4
  address at the same time: `vip` resolves to IPv6 on the Mac, and namchaind and pedald listen on
  IPv4 (`0.0.0.0`). The library rigpa reads is the Mac's `~/Documents/Audio`, which
  `sync-library` mirrors, so ids match. Checked read-only: vip's running rig (`last.json`'s
  t5-lstm, its model missing on vip, as the engine says), live status, and an empty rig library
  (`GET /library/rigs` agrees: only the resume snapshot is there). Edits made this way are real:
  they play on the pedal and auto-save into vip's library.
- Found by the dev loop and fixed after M8: a cab's low cut saved at its 10 Hz minimum stopped
  namchaind starting (namchain `784d8af`: the biquad now takes 10 Hz, and a test holds every
  macro's range inside its targets'), and rigpa now honours `min_inclusive`/`max_inclusive`
  (`8df29fc`).
- **M0–M8 are done (2026-10-09).**
- Also asked for and done: a double tap on a knob resets it to its default.
- **M8 done (2026-10-09).** Keeping the library in step (`RigManager`):
  - Auto-save: a second after the last edit made here, the engine's document (`rig.get`) is PUT
    to the library rig that is running. The header says saved / saving… / not in the library /
    not saved. Edits still on their way delay it; Rename and Copy wait for it.
  - Under the rig list (touch): New, Rename, Copy, Delete, on the running rig; Save as… when the
    running rig isn't in the library. Each asks in a dialog (`RigDialog.qml`, an overlay whose
    ring is OK and Cancel); names are checked as pedald checks them, and against the list (any
    case). New loads the new rig. Renaming (or saving as) the running rig applies the document
    under its new name: a name-only `rig.apply`, which the engine takes without a rebuild
    (checked: the generation stays), and the history's snapshots take the name too.
  - Qt Virtual Keyboard: `QT_IM_MODULE=qtvirtualkeyboard`, the `InputPanel` 880 wide at the
    bottom, clear of a dialog's name row.
  - The footer now says what the rig has to say (a refusal, a failed save); a refused apply no
    longer leaves an undo step.
  - Races found and fixed: the rig list dropped a refresh asked for while one was out (a deleted
    rig could come back for a poll); the running rig was re-derived from a status older than an
    apply in flight (a rename read as "not in the library" for a moment).
  - `--input` replays once the rig is on screen (it used to fire at 1.5 s, before a slow start).
  - Tests: `tst_manage_live` (namchaind and pedald, both real): auto-save, name checks, rename of
    the running rig without a rebuild, copy, delete of the running rig, save as, new, cancel.
    16 suites, green five runs in a row.
- **M7 done (2026-10-09).** Adding, removing, lanes, undo:
  - The palette (`PaletteModel`, `AddPanel.qml`, the header's +): Amp (a NAM model and a tone
    stack, one element, as ljoss makes it), Parallel split (two empty lanes), and every engine
    type that isn't plumbing. A model or IR it needs is the library's first, changed in its
    editor. It goes in after the selected block (in its lane), else at the end of the first
    chain, and is selected. New ids are ljoss's: a UUID element, nodes `<element>-<suffix>`.
  - Remove: the node editor's Remove (now also a stop on its ring, after Close), and the drag
    trash.
  - Sections: their editor has Lanes (2–4; fewer takes lanes off the end with what is in them)
    and, per lane, Level, Mute and Invert. These are ljoss's lane gain: a trailing
    `nc.gain` `<lane>-gain` with a linear gain (negative inverts, 0 mutes; a muted lane keeps its
    level and polarity in the node's `meta`), because the mixer's per-port values aren't document
    fields. The tree hides that node from the lane and writes it back. A lane level streams once
    the node exists; the first change adds it with an apply.
  - Undo/redo (`RigSession`): document snapshots, recorded before every edit made here (a run of
    one parameter is one step); undo and redo apply a snapshot with `rig.apply`. Cleared when
    another rig is loaded or the rig's name changes. Header buttons and ⌘Z / ⇧⌘Z.
  - The engine's budget is the cost check: an apply over it is refused (`over_budget`) and the
    footer says "Too much for the pedal: …"; nothing calls `rig.cost` first.
  - QML names to avoid: `Palette` (a Qt Quick type) and the id `palette` (every Item's property).
  - Tests: lane counts and gains, new sections (`tst_edits`); live: lanes and undo/redo, Remove
    and undo, the palette's every row, Amp refused over budget then accepted; ⌘Z (`tst_devinput`).
- **M6 done (2026-10-09).** Drag and drop:
  - `RigEdits` (`src/rig/RigEdits.*`): move, insert, take and remove on the editor's tree, and
    the tree written back as an engine document (every node object, chain key and document key
    kept; a bare graph stays bare). A place is a chain, a path of `<section element>/<lane>`, and
    a slot counted without the moving element.
  - `RigEditorModel`: lift, dragTo, drop, dropOnTrash, cancelDrag. While lifted, the layout is the
    document *with the move already made*, so the other tiles step aside, rows re-wrap, and a
    section grows round a lane drop; the drop applies exactly that. Hit testing: the deepest
    container (a lane, else a chain, else the nearest chain) and ljoss's `flowhit` within it.
    A layout of the same tiles updates delegates in place (they glide, 140 ms), anything else
    resets the model.
  - `RigSession::applyDocument`: one `rig.apply`, shown at once; reads that started before the
    apply are ignored until it is answered; a refusal is said in the footer and the engine's
    document read back.
  - QML: a long press lifts (a finger; a mouse or trackpad drags at once), a ghost follows the
    pointer, a Remove zone appears bottom right, and the view auto-scrolls near its edges
    (900 px/s × depth²). Block looks moved into `TileFace.qml`, shared with the ghost.
  - `--demo-drag from:to` holds a lifted node over another, for screenshots.
  - Tests: `tst_edits` (round trip, moves within, into and out of lanes, refusals, removal) and
    two live cases (a drag reorders what the engine runs; the trash removes).
- **M5 done (2026-10-09), awaiting the owner's review.** The node editor:
  - `Schema` reads `engine.schema` once per connection; `NodeEditorModel` builds one control per
    visible, patchable parameter of the block's nodes, in reading order: knobs for numbers
    (logarithmic where the schema says, open ends given a stand-in by unit), segmented choices for
    up to five options, stepped knobs for more, toggles for flags, and a picker for a model or IR.
    One of a `one_of` group is shown (the one the node sets). The grid shrinks until it fits.
  - The ring is the controls, then Bypass, then Close. Apply on Close, a choice or Bypass acts;
    on a knob it selects it and the rotary turns it. Opened by apply in the rig editor or a tap
    on the selected tile once the 300 ms double-tap window has passed; a double tap bypasses.
  - `EditStream`: `rig.patch` ops, one in flight, at most every 10 ms, a waiting op replaced by
    a newer one for the same parameter. Rampable numbers stream as they move; anything else is
    sent on release, deselect or choice. Primitives are addressed by node, a macro by element
    when its element has one macro (the engine ramps it only then: tried, `smooth` vs a
    structural ramp by node). Bypass is a `bypass` op per bypassable node of the block (by
    element is refused when a block has two nodes). A model or IR is a `set_param` of its
    reference (`swap_model` behaves the same).
  - The picker (`PickerModel`) lists the library's models or IRs, opened on the current one.
  - A parallel section opens an editor that says its lanes come later (M7).
  - Tests: `tst_schema` and `tst_edit_live` (streaming without a new generation, bypass, model
    swap, an option, coalescing a 101-step drag into at most three patches). The live daemon now
    has two channels, so the Dual fixture loads.
  - Uses `QtQuick.Shapes` for the knobs: vip will need `qml6-module-qtquick-shapes` (V1).
  - Not done: parameter summaries on tiles (no room on a 96 px tile beside a two-line title;
    revisit with the real screen).
- Earlier: M4 (rig view), M3 (library, browse), P1 (pedald rig API, pedal.project `70a6d35`),
  M2, M1, M0. `dev-pedal` takes `RIGPA_DEV_DIR` and port overrides, so a second back end can run
  beside yours.


## Why

You have a larger enclosure (Hammond 1456RH3) and an 8.8″ 1920×480 touchscreen (Prechen bar display:
video over mini‑HDMI, power and touch over USB; the panel is natively 480×1920). The pedal gets an
on‑device UI, **Rigpa**, in the role ljoss has on the Mac, running on the Core host (vip) on core 0,
written in C++ with Qt 6. It talks to `namchaind` on 127.0.0.1:47810 with the existing token, reads
NAM/IR files directly and read‑only, and builds and runs on the Mac for day‑to‑day work.

What exploration established, and what it changes:
- **Protocol** (namchain `docs/daemon.md`): u32‑LE length + JSON over TCP; `engine.hello`
  `{protocol:2, token}`; the token is plaintext and constant-time compared. **Nothing is ever pushed.** Clients poll
  `engine.status` and `engine.levels`. TCP connections are capped at 4 (`tcp_max_connections`).
- **Rigs** are not namchaind's: it has no list method and persists nothing. **pedald owns the rig
  library** (`/var/lib/pedal/library/rigs`, D4), and its library API (`GET/PUT /library/rigs…`, v3)
  is designed but not built. Your answer: rigpa saves and manages rigs **through pedald**, so that
  API comes first.
- **vip today cannot show anything.** The trimmed kernel has no vc4/v3d/USB‑HID modules,
  `config-pedal.txt` leaves out KMS on purpose ("GPU memory traffic"), and the image has no
  Mesa or Qt. The token is root‑only (0600), so rigpa takes it via `LoadCredential`, as
  pedald does.
- **The constraint changes.** "The Core host does DSP only; display … belong to the Pico"
  (pedal.project `AGENTS.md`, D2 and about a dozen docs) becomes "the Core host also drives the
  screen (rigpa, core 0); the Pico keeps every physical control". This is recorded as a new
  decision, and it is gated by a soak, because memory traffic from the housekeeping core is the
  documented way to break the render core (namchain `docs/tuning.md`: `stress-ng --vm` → 5,460 xruns).
  Since 2026-10-09 there is exactly one housekeeping core (0), and the audio interrupt is no longer
  on it: it is delivered to core 1, a render core that is never the active one.
- **namchaind and pedald both run on the Mac.** namchain's `build-mac/` has the null/CoreAudio
  backends, and pedald's UART link just retries. So the Mac dev loop is fully local.

## How it is built

**Repo** `rigpa/` (private `github.com/rikkus/rigpa`, ssh remote like ljoss):
```
AGENTS.md  CLAUDE.md(@AGENTS.md)  README.md  .gitignore(/build/ /.qt/ /.dev/)  mise.toml
CMakeLists.txt  CMakePresets.json            # mac-debug, mac-release, linux-release (container)
docs/spec.md  docs/plan.md                   # the spec above; milestones + "Next session starts with"
src/app/      Config Settings Navigator DevInput AppController  main.cpp
src/engine/   Framing EngineClient EngineSession Schema EditStream
src/pedald/   PedaldClient                   # QNetworkAccessManager, bearer token
src/library/  NamSniff WavInfo MakeHeuristics LibraryScanner LibraryModel
src/rig/      RigDocument RigEdits RigEditorModel NodeEditorModel History RigSaver RigListModel
src/layout/   FlowLayout HitTest AutoScroll  # pure, unit-tested
qml/          Main Shell(Menu Header Footer InfoPanel) views/(RigView RigList RigEditor NodeTile
              Wires SectionFrame NodeEditor Palette Picker BrowseView SettingsView)
              controls/(Knob Toggle Segmented Stepper Meter MenuButton) Theme(singleton)
resources/icons/  rigpa.svg (ljoss's icon, flattened: QtSvg may not resolve SVG2 <use href>) folder settings undo redo power
tests/        Qt Test + CTest; fixtures/ (rig docs, a schema reply, .nam heads/tails, wav headers)
dev/          dev-pedal (script), rigs/ (fixture rigs), config templates
```
- **One static library `rigpa_core`** holds every non‑QML class. The app and the tests both link
  it.
- QML lives in `qt_add_qml_module(URI Rigpa)` and is compiled ahead of time with qmlcachegen.
  C++ types are exposed with `QML_ELEMENT`/`QML_SINGLETON`.
- C++20, matching namchain.

**Ports from ljoss** (Swift, no UI code in these files, so they translate directly). Each comes with
its tests:

| rigpa | ljoss source |
|---|---|
| `Framing` | `Engine/EngineFraming.swift`, tests `Tests/WireTests.swift` |
| `EngineClient`/`EngineSession` (backoff 1/2/5/10/30 s; no retry on `unauthorized`/`unsupported_protocol`; 10 s reply timeout; status 2 Hz, levels 10 Hz) | `Engine/EngineClient.swift`, `EngineSession.swift`, `EngineLevels.swift`; namchain `tools/session/nc_session.py` for the async shape |
| `EditStream` (rampable → `set_param` ≤10 ms, one in flight, last value; else on release; primitives by `node`, macros by `element`; a param answered `structural` stops streaming) | `Engine/EngineEdits.swift`, `docs/rig-edits.md` |
| `FlowLayout` (line breaks, closed gaps never start a row, the four wire rects per break) | `DragEngine/FlowGeometry.swift`, `Views/ChainCanvas.swift:88-151`, tests `Tests/FlowTests.swift` |
| `HitTest` (deepest container; rows by vertical overlap; gutter split at midpoint; first mark with `midX` right of the pointer) | `DragEngine/FrameRegistry.swift:70-143`, `FlowGeometry.swift:130-191` |
| `RigEdits` move + normalize (render→model index, then move; both steps or it shuffles one place) | `DragEngine/RenderSlotMapping.swift`, `Model/ChainEdits.swift:79-103,191-224` |
| `AutoScroll` (edge zone min(78, viewport/4), 900 px/s × depth², 16 ms tick, recompute the target every tick) | `DragEngine/AutoScroll.swift:85-172` |
| `NamSniff`, `WavInfo`, `MakeHeuristics` | `Assets/NAMSniff.swift`, `AssetScan.swift`, `MakeHeuristics.swift`. **Also scan the tail for `metadata`**: in 0.7 files it sits after the weights, which ljoss's 16 KiB head misses |
| Visual constants scaled to a 368 px content (two tile rows visible) | `Views/DesignSystem.swift:8-87` |

**Rig model.**
- `RigDocument` turns `rig.get`'s document into, per chain, a series of elements:
  - a **block** is consecutive nodes sharing an `element`, so ljoss's NAM + tone stack are one
    tile;
  - a **section** is an `nc.split`/`nc.mix` pair with its lanes.
- It turns the tree back into nodes and connections, keeping every node object, its params and
  `meta`.
- A chain whose graph isn't split/mix-shaped is shown read‑only.
- New nodes get a UUID `element` and an `<element>-<suffix>` id, as ljoss does, so the two stay
  compatible.
- Tile labels come from the model or IR display name (library metadata) or the schema type name.
  ljoss's catalogue names aren't in engine documents.

**Edits to the engine.**

| Change | How it reaches the engine |
|---|---|
| Parameters | `EditStream` |
| Bypass | `rig.patch` `bypass` per node of the tile (a crossfade, no rebuild) |
| Model/IR | `rig.patch` `swap_model` |
| Move, add, remove, split | `rig.apply` of the rewritten document, with `rig.cost` first when cost can rise (refusals shown in the footer) |
| Undo | the same paths in reverse (`History`) |

**Following others.** When `engine.status` shows `rig`, `generation` or `params_applied` move
without rigpa causing it, rigpa re-reads `rig.get`. `GET /library/rigs` is polled every 5 s and
after each of rigpa's own changes.

**Navigation.**
- `Navigator` is a C++ state machine (main view, rig focus list/editor, overlay, highlight,
  editing). the spec's table is its test oracle.
- Its inputs are `rotate(n)`, `apply()`, `back()` and `touched(target)`.
- `DevInput` is an application event filter:
  - ⌘+wheel feeds `rotate`, one step per ~30 px of trackpad delta or 120 units of mouse wheel;
  - `.` and `,` feed apply and back, and are skipped while a text input has focus.
- Later, a `PedalInput` adapter feeds the same three calls.

**Toolchain (Mac, project-owned).**
- `mise.toml` pins `pipx:aqtinstall`, `cmake` and `ninja`.
- `mise run qt` installs Qt **6.10.2** (the version vip's Ubuntu 26.04 ships) into `.qt/` with
  `aqt install-qt mac desktop 6.10.2 clang_64` plus the add-on modules rigpa uses (Virtual
  Keyboard, and any `aqt list-qt … --modules` shows are not in the base).
- The presets point `CMAKE_PREFIX_PATH` at `.qt/`; Homebrew's Qt is never used.
- Tasks: `qt`, `build`, `test`, `run`, `dev`.
- On Linux, the same CMake builds in the `pedal-build` container against Ubuntu's Qt 6.10.2.

**Dev loop.** `dev/dev-pedal` (`mise run dev`) runs the whole back end locally under `.dev/`:
- It generates one `namchaind.json`, used by namchaind (`--config`) and by pedald
  (`--engine-config`). In it:
  - the null backend, or `coreaudio` with a looped WAV when the meters should move;
  - TCP 127.0.0.1:47810, with a 64‑hex token kept in `.dev/token` (mode 0600);
  - `rigs_root=.dev/library/rigs`, seeded from `dev/rigs/`;
  - models/irs roots at `~/Documents/Audio/{NAM,IRs}`, which is what `sync-library` mirrors, so
    the ids match vip's.
- It starts `../namchain/build-mac/namchaind`.
- It starts pedald with `python3 -m pedald run`, its `PYTHONPATH` at
  `../pedal.project/devices/common/rootfs/usr/local/lib`, and a `uart_device` that doesn't exist
  (the link just retries).
- It writes `.dev/rigpa.json`.
- Pointing the Mac build at vip over WiFi is M9, not part of the dev loop: nothing before M9
  touches vip.

**Config and settings.**
- `rigpa.json` is found via `--config`, then `$RIGPA_CONFIG`, then the platform default. It holds:
  - `engine_config` and `pedald_config` paths, read for ports, token file and roots (vip);
  - explicit `engine`, `pedald` and `library` overrides (Mac);
  - `settings_file`;
  - `display.rotation` and `display.scale`.
- The token is read from `$CREDENTIALS_DIRECTORY/token` first, then the token file, as pedald
  does.
- Settings are `QSettings` INI: `rig_list_width` and `live_meters`. On vip they live in
  `/var/lib/rigpa`, which moves to the data partition with Phase 4's overlayroot.

## Milestones

- **Before starting:** pull pedal.project (and any repo the step touches) to `origin/main`, then
  run `scripts/status`.
- **M0 splits this file into its homes:**
  - the spec becomes `rigpa/docs/spec.md`;
  - the rest becomes this file, with a "Next session starts with";
  - "Pedal-level records" and P1 go into pedal.project.
- **Order:** each milestone is green on its own and lands in dependency order, service before app.
- **Commits** go to `main`, with no attribution lines. Pushes wait for your say‑so.
- **Review pauses:** after each of M0–M8, stop and demo the UI running on the Mac (screenshots
  plus how to run it yourself), and wait for your review before starting the next milestone.
- **vip gate:** before starting M9 (the first work that touches vip), ask whether it's okay to
  start work on vip, and wait for a yes.

| # | Repo | Delivers | You can check |
|---|---|---|---|
| M0 | rigpa, pedal.project | Repo, workspace registration, docs (spec, plan, D14), toolchain, an empty 1920×480 window | `scripts/workspace check` clean; `mise run qt build test run` shows the window |
| M1 | rigpa | Shell: menu (slot 1 blank in rig view), header/footer/info frames, views switching, `Navigator`, `DevInput`, settings view + persistence | `.` `,` ⌘‑scroll drive highlight/edit in settings; width persists |
| M2 | rigpa | Engine client + session, dev loop, info panel (connection, health, DSP, xruns, meters), header rig name | `mise run dev` + `run`: connected, live status (local only) |
| P1 | pedal.project | pedald rig library API (below) + tests; `sync-library`'s rigs re‑own bug fixed | `devices/common/test-pedald` green; curl against the dev pedald |
| M3 | rigpa | Library scan + browse view (Models/IRs, metadata, grouped, rotary up/down) | Browse the Mac library smoothly (~1,500 IRs) |
| M4 | rigpa | Rig view: rig list via pedald, read‑only editor (tiles, wrap, lanes, wires), select, startup rule, rig switching, following others | Switch rigs by tap and rotary/apply; `namchainctl load` elsewhere → rigpa follows |
| M5 | rigpa | Node editor (schema‑driven knobs/toggles/segments, fits without scrolling), bypass switch + double‑tap, model/IR picker, `EditStream` | Knob drags stream (`params_applied` moves, `generation` doesn't); bypass; swap a model |
| M6 | rigpa | Drag and drop: lift, live gap and re‑wrap, lanes in/out, trash, auto‑scroll, ghost; `rig.apply` on drop | Reorder across rows and into lanes; the engine plays the new order |
| M7 | rigpa | Palette (add node, parallel split), remove, split editing (lanes 2–4, per‑lane level/mute/phase), undo/redo | Build amp→split→cab, undo to the loaded rig, redo |
| M8 | rigpa | Auto‑save via pedald, rig management (new/rename/duplicate/delete), Qt Virtual Keyboard | Edit, restart the dev loop, change persisted; rename/delete visible in `GET /library/rigs` |
| M9 | rigpa | The Mac build against vip over WiFi: a second `rigpa.json`, the token fetched once by `ssh vip sudo cat /etc/namchain/token` into a 0600 file (uses one of vip's 4 TCP slots); read‑only use, no deploy | rigpa on the Mac shows vip's running rig and live status |
| V1 | rtpi, pedal.project | vip bring‑up (below) | rigpa runs on vip, viewed over VNC |
| V2 | pedal.project | The D14 gate: soak with rigpa off/idle/meters/animating + RSS | `soak-summary.py` passes in every case |
| V3 | pedal.project, rigpa | The screen: EDID mode, rotation, touch mapping, USB power | Touch works the right way up |
| H1 | firmware, pedal.project, rigpa | Rotary/apply/back over UART → pedald → rigpa (specified in M0, built later) | The knob drives the same table as ⌘‑scroll |

**P1, the pedald API** (pedal.project; `docs/pedal-service.md`'s "upload API" grows to match):
- **Endpoints.**
  - `GET /library/rigs`: names and modification times, `running`, sorted.
  - `GET /library/rigs/<name>`.
  - `PUT /library/rigs/<name>`: stored, not applied. The document's `name` must equal the path
    name. Overwrite is the default.
  - `DELETE /library/rigs/<name>`: removes the rig from `bank.json`.
  - `POST /library/rigs/<name>/rename {"to": …}`: renames the file and the document's `name`, and
    updates `bank.json`.
- **Rules.**
  - The design's name rules; `last` and `last.prev` are refused.
  - Writes are atomic: dot‑temp, fsync, rename, fsync the directory.
  - The bank's cost verdicts are invalidated after any change.
- **`httpd.py`** gains prefix routes (today they are exact paths only, `_resolve`) and `DELETE`.
- Tests go in `devices/common/tests/test_library.py` and `test_httpd.py`.
- It reaches vip with V1's apply.

## Pedal-level records (in M0, pedal.project)

- **Registration.** `gh repo create rikkus/rigpa --private`, then
  `scripts/workspace add git@github.com:rikkus/rigpa.git rigpa`, which appends to
  `workspace.repos`.
- **Workspace files.**
  - `workspace-root/{.pedal-workspace,AGENTS.md}` tables say "six repositories", then
    `scripts/workspace apply`.
  - `scripts/status` `MAC_REPOS` gains rigpa.
  - `docs/workspace.md` and `docs/working-across-repos.md` get a rigpa row: the on‑pedal UI; it
    consumes namchain's protocol and pedald's API, and is deployed by `devices/`.
- **D14 in `plan.md`'s decisions.**
  - The screen and the enclosure are in.
  - rigpa runs on the Core host's core 0 and reads the library read‑only.
  - The Pico keeps every physical control (D2 stands), and rotary, apply and back join the UART.
  - Condition: V2's soak. If it fails, the UI's rendering is cut back until it passes.
- **Other docs.**
  - `AGENTS.md`: the architecture and constraints lines rewritten to match, and "Where things
    are" gains rigpa.
  - The other docs that state the old rule get a one‑line "superseded by D14" note, the way
    `tib` was handled. They are `hardware-handoff.md`'s control‑surface heading,
    `pedal-service.md`, `uart-protocol.md`, `pairing.md`, `config-pedal.txt` and plan 3.7.
  - `about.md`'s Qt‑designer open question is answered.
  - `bom.md` gains the screen and the enclosure, from `imaginings/big-enclosure/`.

## vip bring-up (V1–V3)

Nothing touches vip before M9. Each step that touches vip waits for your go‑ahead, and runs under `scripts/host-lock`. Order:

1. **rtpi.** Add `DRM_VC4`, `DRM_V3D` and their KMS helpers, `USB_HID`, `HID_GENERIC` and
   `HID_MULTITOUCH` to `config/vip-keep.fragment`. Re‑trim from a *verified* stock config (the
   current `build/vip/base.config` is suspect), as `-numaemu2-trimmed`. Put the new hashes in
   `devices/vip/image.lock`, then `vip-boot try-kernel`.
2. **pedal.project**, one commit per concern:
   - `config-pedal.txt` gets `dtoverlay=vc4-kms-v3d`, and its comment is rewritten.
   - `devices/vip/packages` gets the Qt 6.10 runtime, Mesa, libinput and fonts:
     - Qt: `libqt6opengl6` holds `eglfs` and `eglfs-kms`; plus the `qml6-module-*` modules rigpa
       imports (`qtquick-shapes` for the knobs, `qtquick-controls` for the name field,
       `qtquick-virtualkeyboard`), `qt6-virtualkeyboard-plugin` (rigpa sets
       `QT_IM_MODULE=qtvirtualkeyboard` itself), and `qt6-qpa-plugins` for VNC.
     - Mesa: `libegl-mesa0`, `libgbm1`, `libgles2`, `mesa-libgallium`.
     - The list is derived from the container build with `ldd` and `qmlimportscanner`, and
       asserted in `build-image` the way `ldd namchaind` is.
   - The `Dockerfile` gets the matching `-dev` packages from the same snapshot.
   - `devices/common/image/build-rigpa`, modelled on `build-namchain`; its tests run on
     `offscreen`.
   - `scripts/deploy-rigpa`, modelled on `deploy-engine`: lock, export the commit, build in the
     container, write `MANIFEST`, install `/opt/rigpa/<short>/` and swap `current`, restart, then
     `components-lock`.
   - `rigpa.service`. rigpa ships a generic unit, and pedal.project adds a drop‑in, as with
     namchaind. Settings:
     - `User=rigpa` with `SupplementaryGroups=video render input`;
     - `CPUAffinity=0`, `Nice=5`, `IOSchedulingClass=idle`;
     - `LoadCredential=token:/etc/namchain/token`;
     - `ProtectSystem=strict`, `StateDirectory=rigpa`;
     - `OOMScoreAdjust=500`, with `MemoryMax` set from V2;
     - `QT_QPA_PLATFORM=eglfs` and `QT_QPA_EGLFS_KMS_CONFIG=/etc/rigpa/kms.json`, which picks
       the vc4 card by path (the Pi 5 has two DRM nodes); cursor hidden;
     - `After=namchaind pedald`.
   - `sysusers.d/rigpa.conf`.
   - `pedal-device`'s unit list gains rigpa.
   - vip's `namchaind.json` gets `tcp_max_connections` 8.
   - `make-card`, `image.lock`, `components-lock` and `status` learn rigpa.
   - Then `make-card --remote`.
3. **See it before the screen arrives.**
   - A one‑off run with `QT_QPA_PLATFORM=vnc` and `QT_QUICK_BACKEND=software`, viewed from the Mac.
   - Or `rigpa --screenshot` on `offscreen`.
4. **V2 gate.**
   - With the heaviest bank rig, alternate 10‑minute runs: rigpa stopped, idle, idle with meters,
     and animating. Animating means `--demo-animate`, which scrolls and drags in a loop.
   - Measure with `soak-monitor.sh` and `soak-summary.py`. The gate is 0 xruns, 0 overruns and
     worst block under 90%, plus a p99 comparison.
   - Record rigpa's PSS and `MemAvailable`.
   - Evidence goes in `pedal.project/bench-results/<date>-rigpa-soak/`.
   - If it fails, the fallbacks in order are: meters off, fewer animations, then a lower frame rate
     or a static screen. `CPUAffinity` is not one of them: with one housekeeping core (0) and cores
     1–3 rendering, there is no second housekeeping core to move to (2026-10-09).
5. **V3, when the screen arrives.**
   - Read the HDMI EDID mode and set it in `kms.json`.
   - Set `display.rotation` (90 or 270).
   - Check that USB touch lands where pressed. QML rotation maps touches when the digitiser
     matches the native scan; if not, use an evdev `rotate=` or a libinput calibration matrix.
   - Keep the screen's USB draw inside the Pi's 1.6 A.

## Verification

- **Every milestone.**
  - `mise run test` (CTest). It covers unit tests for framing, client, edit stream, document
    round‑trips (ljoss docs with splits and lanes, dual chains, bare graphs), flow layout and hit
    testing (ported FlowTests), move/normalize, navigator table, dev input, NAM/WAV sniffing,
    history, saver.
  - Integration tests against a real null‑backend namchaind and pedald on kernel‑chosen ports,
    modelled on namchain's `tests/cli/run_ctl.cmake`. They are skipped when the binaries are
    absent.
- **By hand on the Mac.** `mise run dev` and `mise run run`, then walk through the milestone's
  "You can check" with the trackpad, `.`, `,` and ⌘‑scroll. I'll take screenshots of each view
  as I go.
- **pedald.** `devices/common/test-pedald`.
- **vip.** V2's numbers decide D14; V3's checklist decides the screen.

## Risks and open items

- **Display memory traffic vs the render core.** This is the reason for the old rule. V2 decides,
  and the screen stays static when idle so the cost is mostly scanout.
- **Memory.** vip has 1 GB; target rigpa under about 150 MB PSS, measured in V2.
- **ljoss vs the pedal.** ljoss re‑applies its own rig on every connect (R6). Until that is
  decided, using ljoss overwrites what rigpa shows, and rigpa follows.
- **Renaming the running rig** needs a name‑only `rig.apply`. It is expected not to replan any
  chain; to verify in M8.
- **The 2.4″ ST7789 module's job (plan 3.7) is superseded.** Its EC11 could become the main rotary
  under H1; that is yours to decide.
- **Qt Virtual Keyboard is GPLv3/commercial.** That is fine for a private build.
