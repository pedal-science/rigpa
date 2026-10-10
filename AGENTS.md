# rigpa

The pedal's own UI: a C++ and Qt 6 (Qt Quick) app that runs on the Core host (vip) on core 0,
fullscreen on the pedal's 1920×480 touchscreen. It plays ljoss's role on the pedal itself: it
talks to `namchaind` over TCP on 127.0.0.1, manages rigs through `pedald`'s HTTP API, and reads
the NAM and IR library directly, read-only. It also builds and runs on the Mac against a local
`namchaind` and `pedald` (the dev loop). What it does: [docs/spec.md](docs/spec.md). How it gets
built, milestone by milestone, and where it stands: [docs/plan.md](docs/plan.md), starting at
"Next session starts with".

## Build and test

The project owns its Qt on the Mac: `mise run qt` installs Qt 6.10.2 (the version vip's Ubuntu
26.04 ships) into `.qt/` with aqtinstall. Homebrew's Qt is never used.

```sh
mise install            # aqtinstall, cmake, ninja, pinned in mise.toml
mise run qt             # Qt 6.10.2 into .qt/ (once)
mise run build          # cmake --preset mac-debug && cmake --build --preset mac-debug
mise run test           # ctest --preset mac-debug
mise run dev            # namchaind (../namchain/build-mac) and pedald (../pedal.project) under .dev/
mise run run -- --config .dev/rigpa.json
```

On the Mac, `.` is apply, `,` is back, ⌘ + trackpad scroll is the main rotary, and ⌘Z / ⇧⌘Z
undo and redo. Names are typed on Qt Virtual Keyboard, on the Mac too.

For screenshots and checks: `--screenshot <png>`, `--view rig|browse|settings`, `--input <keys>`
(replayed once the rig is on screen; `rigpa --help` lists the keys), and `--demo-drag from:to`.
The live tests start their own `namchaind` and `pedald` on ports the kernel picks; a second dev
back end beside yours: `RIGPA_DEV_DIR=$PWD/.dev-claude RIGPA_DEV_ENGINE_PORT=47811
RIGPA_DEV_PEDALD_PORT=47821 dev/dev-pedal`.

Against the pedal itself (vip, over WiFi): `mise run vip` writes `.vip/rigpa.json` (fetching the
engine token once over ssh), then `mise run run -- --config .vip/rigpa.json`. Edits there are real:
they play on the pedal and save into its library.

## Constraints

- QML is for views, gestures and animation only. Protocol, models, layout maths, navigation
  and settings are C++ in `rigpa_core`, which the tests link without a window. Keep it so.
- Target Qt 6.10 exactly (vip's). Nothing newer, even though Homebrew has it.
- The library is read-only to rigpa. Rigs are written only through `pedald` (it owns the rig
  library, pedal.project D4).
- On vip rigpa is SCHED_OTHER on core 0, never real-time; the screen stays static when idle.
  Anything that animates continuously is a cost the D14 soak has to pass.
- Nothing in this repo touches vip before plan milestone M9, and M9 starts only with the
  owner's go-ahead. After each of M0–M8, stop and demo the UI on the Mac for review.
- The protocol is namchain's (`../namchain/docs/daemon.md`); the rig library API is
  pedald's (`../pedal.project/docs/pedal-service.md`). Change them there first.

System context (the pedal, how the repos fit together, the plan): `~/code.local/pedal/pedal.project`, starting at its `CLAUDE.md` and `docs/working-across-repos.md`.
