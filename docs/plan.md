# Rigpa: plan

How [spec.md](spec.md) gets built. Agreed 2026-10-09.

## Next session starts with

- **M0 done (2026-10-09), awaiting the owner's review.** `rikkus/rigpa` (private) is registered in
  the workspace (`scripts/workspace check` passes); pedal.project `876715e` records D14 and the
  sixth repository. `mise run qt` installs Qt 6.10.2 into `.qt/`; `mise run test` is green; the
  app is an empty window with `--screenshot <file>`.
- On a laptop screen the 1920×480 window is clamped to the screen's width: M1's `display.scale`
  fixes that.
- **Next: M1** (shell, navigation, settings) once M0 is reviewed. Nothing touches vip before M9,
  and M9 starts only with the owner's go-ahead.

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
  decision, and it is gated by a soak, because memory traffic from the housekeeping cores is the
  documented way to break the render core (namchain `docs/tuning.md`: `stress-ng --vm` → 5,460 xruns).
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
     - Qt: `libqt6opengl6` holds `eglfs` and `eglfs-kms`; plus the `qml6-module-*` modules,
       `qt6-virtualkeyboard-plugin`, and `qt6-qpa-plugins` for VNC.
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
   - If it fails, the fallbacks in order are: meters off, fewer animations, `CPUAffinity=0-1`,
     then a different render path.
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
