# SoC on OpenXRay: handover

State of the Shadow of Chernobyl 1.0006 work (issue
[#392](https://github.com/OpenXRay/xray-16/issues/392)) on branch `shoc-392`, as of
2026-10-14. Background and design decisions: [shoc-compatibility.md](shoc-compatibility.md).
Test runs and findings: [shoc-smoke-test-log.md](shoc-smoke-test-log.md).

## Goal and rules

The goal is to make SoC 1.0006 bootable, playable and finishable on OpenXRay without
breaking CoP, CS or CoC. The rules from the original brief and from the owner:

- Stay close to vanilla behaviour, no gameplay changes. The reference is the original
  game (the owner compares screenshots of the original with this build).
- Don't modify original game resources (`.db` archives, gamedata).
- Gate every SoC change behind `ShadowOfChernobylMode` (or a data-driven check). Don't
  gate on `ClearSkyMode` without verifying CS data.
- Ask before large architectural changes.
- Small commits, each referencing `(#392)`. Generic OpenXRay fixes (all games) get their
  own commit without the SoC prefix.
- Nothing is pushed or turned into a PR without asking the owner.
- The owner communicates in German; answer in German.

## Where things are

| What | Where |
|---|---|
| Branch | `shoc-392`, based on `dev`; fork remote `origin` = github.com/Itsgopnik/xray-16 |
| Design note | `doc/design/shoc-compatibility.md` |
| Test log | `doc/design/shoc-smoke-test-log.md` |
| Smoke test | `misc/tests/smoke/run_smoke.sh`, driver `misc/tests/smoke/smoke_driver.lua` |
| Data dumper | `misc/tests/tools/dump_scripts.lua` (scripts/configs/textures from the encrypted archives, local analysis only, never commit game data) |
| SoC script shims | `src/xrGame/script_shoc_compat.cpp` |
| SoC PDA windows | `src/xrGame/ui/UISHoC*Wnd.*`, `UIEncyclopediaWnd.*`, `UITreeViewItem.*`, `UIEncyclopediaArticleWnd.*`, `UISHoCPdaAux.*` |

The owner's local machine (Linux, CachyOS, AMD GPU, OpenGL renderer) also has, outside
the repo:

- the SoC install `~/dev/shoc-dev` (Steam 1.0006 archives, `fsgame.ltx`, launch script
  `play-shoc.sh [--rebuild]`);
- working data `~/dev/shoc-work`: OGSR sources used as port references (`ogsr-ui/`),
  dumped original XMLs (`shoc-ui/`), scripts, configs, smoke runs;
- reference screenshots of the original game in the Steam screenshot folder.

A remote environment has none of this: it can read, change and build the code, but it
can't run the game or the smoke test (no game data). Runtime checks then have to be done
by the owner locally.

## What was done (88 commits on top of `dev`)

### Engine, detection, test infrastructure

- `EGameType` enum and `game_mode = auto` detection (encrypted SoC archives).
- `-test_script <lua>` hook (non-MASTER_GOLD builds) and the smoke test: phases `menu`,
  `play`, `load` (default), optional `pda`, `dogs`, `loot`, `farspots`, `mainmenu`,
  `music`. `--headless` runs each phase in an invisible `kwin_wayland --virtual` session.
- Script/config dumper and missing-export finder; SoC script shims with call site logs.
- Linux fixes: `-silent_error_mode`, fatal signal handlers, `strncpy_s`, `-nogame` match,
  ASan build support.

### Gameplay/runtime compatibility (SoC data)

- Artefact slot key, upgrade manager, tutorials, tracer and water textures, month names,
  weather, level map checks: tolerant handling of SoC data.
- Monsters: dog group animations and missing velocities fall back; monster
  smart-terrain states keep a copy of SoC's temporary Lua task (dangling pointer crash).
- Physics: ragdoll weapon collision removed when the weapon leaves without a shell.
- Stalkers keep their PDA in the PDA slot (contacts).
- Task objectives get their own map locations (hidden ones after the previous
  objective); the task's own article is registered.
- News and talk icons keep SoC's texture rect (encoded in the texture name, save
  format unchanged).
- Level music: SoC's selection without midnight wrap. Its night tracks are cut music
  (amb20, amb09, amb17) that the original never played; the owner wants to keep it that
  way (see commit `7a0066ff3` for the list).

### PDA (all seven tabs ported from the original layouts, screenshot-verified)

Tasks, map, diary, contacts, ranking, statistics, encyclopedia; SoC tab bar with
"new content" signs; map scroll bars, zoom rule, map name label, correct bound rect for
the current level (spots were drawn next to the actor); tree items without flicker;
article images (generic XML path fix).

### HUD

Ammo name and fire mode on the weapon frame; minimap PDA contacts counter, distance
format `583.1 m.` and compass position; dead body search shows the partner frame for
bodies, not for boxes.

### Main menu and options

Options open without assertion (default tab ids), first tab pushed; check boxes keep
the checker icon size; buttons stretch only with `stretch="1"` (tab captions); key
binding header captions; renderer caption "Dynamische Beleuchtung"; version
`ver. 1.0006`.

### Generic OpenXRay fixes (affect all games, own commits)

- `d8f08ef16` XML paths with empty tokens (`":texture"`) work again: article images.
- `3947d6d8d` animated static frame row (PDA header animations jumped back halfway).
- `8f92d40aa` only deliberate controller input switches the input type: stick noise of a
  connected but unused DualSense hid the mouse cursor in message boxes.

## Owner's install changes (local only)

- `ui_style ui_style_default` in `user.ltx` (was `ui_style_shoc`, which overrides
  layouts the ported windows read from the original XMLs).
- In `gamedata/config/openxray.ltx` three CoP-template settings are commented out
  (`default_to_slot`, `minus_one_slot_ordering`, `allow_nil_conversion`); a stray
  `scroll_bar.xml` was disabled.

## Open points

- Not yet verified by the owner after the last build: options tab captions, first tab
  highlight, mouse cursor with the DualSense connected (the controller must still work
  when used).
- Parked by the owner: a ragdoll stuck stretched between roof beams (needs a save right
  before it; may be vanilla X-Ray behaviour).
- Double click on the already active objective doesn't deactivate it (no task manager
  API; SoC did).
- `res/gamedata/configs/ui/styles_/ui_style_shoc` is obsolete for SoC and could be
  removed (ask first).
- R4 renderer and the Windows build are untested (Linux-only environment so far).
- PR to upstream: later, on the owner's request. Good candidates: the three generic
  fixes above.
- The owner said there is "still a lot" to compare; HUD and main menu are done, the next
  area is the owner's choice.

## Working method that proved useful

1. The owner sends a screenshot of the original and of this build.
2. Find the original logic: the dumped SoC XML/scripts and the OGSR sources (OGSR is
   based on SoC 1.0006 code), e.g.
   `https://raw.githubusercontent.com/OGSR/OGSR-Engine/master/ogsr_engine/xrGame/<file>`.
3. Change OpenXRay behind `ShadowOfChernobylMode`.
4. Locally: build with `cmake --build bin -j16` (without `-j` it runs one job), run the
   matching smoke phase with `--headless`, compare the screenshot, run the default
   regression (11 PASS), commit.
5. Before every build or engine run check that the owner's game isn't running
   (`ps -eo args | grep "bin/x86_64/Debug/xr_3da"`, theirs has no `-test_script`).
   Engine runs outside `run_smoke.sh` must back up and restore `_appdata_/user.ltx`.
