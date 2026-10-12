# SoC smoke test log

Repeatable smoke test for issue [#392](https://github.com/OpenXRay/xray-16/issues/392).
Design background: [shoc-compatibility.md](shoc-compatibility.md).

## Procedure

```sh
misc/tests/smoke/run_smoke.sh --game <SoC install with fsgame.ltx> --bin <dir with xr_3da> \
    [--out <dir>] [--timeout 300] [--phases "menu play load"] [--headless] [--gdb] \
    [-- <extra engine args>]
```

Use `--headless` on a desktop machine: each phase then runs in an invisible
`kwin_wayland --virtual` session (Xwayland, hardware OpenGL, private D-Bus), so the
game neither shows a window nor steals focus, and stray input can't reach it.
`--gdb` runs the engine under gdb and writes all thread backtraces of a crash to
`<phase>.stdout.log`.

Each phase is a separate engine process, driven by `misc/tests/smoke/smoke_driver.lua`
through `-test_script` (Debug/Mixed builds only):

| Phase | Steps |
|---|---|
| `menu` | Boot to the main menu, quit from the menu |
| `play` | `-nogameintro -start server(all/single/alife/new)`: new game, wait 20 s, kill the nearest living enemy without story id (`obj:kill(actor)`), save as `smoke_test`, `load smoke_test` in the same process, check the enemy is still dead, quit |
| `load` | `-start server(smoke_test/single/alife/load)`: cold load, check the enemy is still dead, quit |
| `pda` (optional) | New game, open all seven PDA tabs via `ActorMenu.get_pda_menu()`, take a screenshot of each (moved to `<out>/screenshots`) for comparison with the original game |
| `dogs` (optional) | New game, `ai_ignore_actor 1`, `g_god 1`, actor moved next to the nearest dog pack for 90 s so the group states (rest, idle animations) run, quit. The report counts dog animation fallbacks |

A phase fails on any driver `FAIL`, a non-zero exit code, a timeout, or a missing
`KERNEL:QUIT` in the engine log. The runner backs up and restores the engine log and
`user.ltx`, moves the `smoke_test` save into the output directory, and writes
`results.txt` and `report.md` there.

Limitations:

- The enemy is killed by script, not by shooting. Weapon handling is not covered.
- Without `--headless` the game window takes focus, and mouse or keyboard input that
  reaches it is processed by the game (that is how the two crashes below were found).
  It also skipped the opening cinematic in some runs, which is why the play phase now
  always passes `-nogameintro`.
- Clean shutdown from the main menu is tested; shutdown via the in-game menu is not.

## Run 2026-10-11 (Linux)

- Engine: `shoc-392` branch, Debug build 10108 (commits up to `52e1ea2de`)
- OS: CachyOS, kernel 7.2.9
- GPU: AMD Radeon RX 9070, Mesa 26.2.4, OpenGL 4.6; renderer `renderer_r3` (default)
- Game: SoC 1.0006 Steam archives (`gamedata.db0`–`dbd`), overlay with
  `game_mode = shoc` and the `task_manager.script` fix
- Game mode detected: `shoc` (autodetection also verified with `game_mode = auto` and
  without `-soc`: `* Game mode: encrypted SoC archives found, using shoc`)

### Results (final build, two consecutive green runs)

    PASS main_menu main menu is updating
    PASS menu_quit clean shutdown
    PASS new_game actor spawned on level l01_escape
    PASS kill_enemy id=380 esc_specnaz2 online alive=false
    PASS save smoke_test.sav written
    PASS load_inprocess actor spawned after 'load smoke_test'
    PASS enemy_dead_after_inprocess_load id=380 esc_specnaz2 online alive=false
    PASS play_quit clean shutdown
    PASS load_cold actor spawned from command line load of smoke_test
    PASS enemy_dead_after_cold_load id=380 esc_specnaz2 online alive=false
    PASS load_quit clean shutdown

| Phase | Exit code | Duration | `!` lines | FATAL |
|---|---|---|---|---|
| menu | 0 | 4 s | 0 | 0 |
| play | 0 | 46 s | 1 | 0 |
| load | 0 | 17 s | 0 | 0 |

Remaining warning: `! SV:ge_destroy: [N] not found on server` (once, during the
in-process load). Not investigated yet.

### Crashes found and fixed during this session

Both were triggered by real input reaching the game window during test runs.

**1. Double click on a slotless inventory item** (fixed in `8a98c33bb`)

```
Expression    : NO_ACTIVE_SLOT != slot
Function      : ItemFromSlot
File          : src/xrGame/Inventory.cpp
CInventory::ItemFromSlot(unsigned short) const
CUIActorMenu::OnItemDbClick(CUICellItem*)
CUIDragDropListEx::OnItemDBClick(CUIWindow*, void*)
...
CUISequencer::IR_OnMousePress(int)
```

`OnItemDbClick` called `SlotIsPersistent()` and `ItemFromSlot()` with `NO_ACTIVE_SLOT`
for ammo or SoC belt artefacts: a VERIFY in Debug, an out-of-bounds read of `m_slots`
in all builds. The code path isn't SoC-specific.

**2. Artefact key in SoC** (fixed in `a4766e8bc`)

```
Expression    : slot <= LastSlot()
Function      : Activate
Description   : wrong slot number. Slot = 11, LastSlot() = 10
CInventory::Activate(unsigned short, bool)
CInventory::Action(unsigned short, unsigned int)
CActor::IR_OnKeyboardPress(int)
```

SoC has no artefact slot. `R_ASSERT`, so this crashed release builds too.

### Not run

- CS 1.5.10, CoP 1.6.02, CoC 1.4.22 regression runs (no installs on the test machine).
  The driver reads the actor binder from `system.ltx`, so it should work there
  unchanged; for CS pass `-- -cs`.
- Windows: `run_smoke.sh` is Linux-only for now.

## Run 2026-10-11, headless (Linux)

Same machine and game data, `--headless`, engine with the script API shims
(`shoc-392` up to `bc13cf39e`).

    PASS main_menu, menu_quit                       (5 s)
    PASS new_game, kill_enemy, save, load_inprocess,
         enemy_dead_after_inprocess_load, play_quit (45 s)
    PASS load_cold, enemy_dead_after_cold_load,
         load_quit                                  (18 s)

### Issues found while building the headless mode

**`-nogameintro` disabled the whole game module** (fixed in `15cfb11fc`):
`entry_point.cpp` checked `strstr(commandLine, "-nogame")`, which also matches
`-nogameintro`. The engine then started without `xrGame` (`! Unknown command: bind`,
no scripts) and `-start` did nothing.

**Error dialogs block unattended runs** (fixed in `438a0ee5e`): on Linux a fatal error
or SIGTERM opened an SDL message box and waited forever. When `timeout` killed the
parent, the engine stayed behind; seven such orphans had accumulated. `-silent_error_mode`
now works outside Windows too, and the runner kills leftover engines after each phase.

**Level start pauses without input focus:** for single player, the engine pauses the
level at start ("application start") if the window has no input focus and
`rs_always_active` is off. Headless runs set `rs_always_active on` for the duration
of the run.

### Physics crash after the kill (fixed in `8e062431b`)

Segfault or pure virtual call in `CPHSimpleCharacter::UpdateDynamicDamage`
(`obj->ObjectGetDestroy()`) in the death frame of the scripted kill, about once in 10
runs. Under gdb the reference object pointer had no valid vtable, and no other thread was
active.

Cause: when a stalker's ragdoll is created, `AddActiveWeaponCollision()` moves the active
weapon's collision geoms onto the hand bone. They keep the weapon as physics reference
object and were only removed when the weapon activated its own shell. SoC's
`death_manager` releases corpse weapons in the death frame ("just before destroy", no
shell), so the ragdoll kept geoms pointing to the deleted weapon until the next hard
impact. Items leaving without a shell now notify the parent, which removes the copies.
Verified: 30 consecutive headless play runs without a crash.

### Further fixes found on the way

- **Missing save / fatal signal during an error dialog** (`41e4091a1`): the fatal signal
  handler returned and the engine ran on behind the failed assert. It now terminates.
- **`xr_terminate` used `exit()`** (`554cd9676`): static destructors ran on the broken
  state and hid the original error (`SFillPropData` `VERIFY(0 == counter)`).
- **AddressSanitizer findings** (both on every start):
  `strncpy_s` read one byte past the source (`19b1026c3`), and the main menu wrote into a
  deleted message box when an MP template was missing (`8a1537fc1`).
- `SV:ge_destroy: [N] not found on server` during the kill is harmless: the engine
  (`CAI_Stalker::Die`) and SoC's `death_manager` both destroy the same ammo.

## AddressSanitizer runs

```sh
cmake -S . -B bin-asan -DCMAKE_BUILD_TYPE=Debug -DXRAY_USE_ASAN=ON -DMEMORY_ALLOCATOR=standard
cmake --build bin-asan -j$(nproc)
ASAN_OPTIONS=detect_leaks=0:detect_odr_violation=0:halt_on_error=1:log_path=/tmp/asan/asan \
    misc/tests/smoke/run_smoke.sh ... --headless
```

Note: all build directories write their binaries to `bin/x86_64/<config>`, so an ASan
build replaces the normal Debug binaries. Rebuild the normal tree afterwards (delete
`bin/x86_64/Debug/*.so` and `xr_3da` first, otherwise make considers them up to date).
`detect_odr_violation=0` is needed because ImGui is linked into both xrEngine and xrGame.
13 ASan play runs with all fixes were clean.

## Run 2026-10-11, dogs phase (Linux)

    PASS dogs_found esc_dog_swarm_0001 (dog_weak), actor moved next to it
    PASS dogs_observed 90 s next to the pack without crash
    PASS dogs_quit clean shutdown

Animation fallbacks used: `stand_idle_howl_0`, `stand_idle_shake_0`,
`stand_idle_smelling_look_around_0`, `stand_idle_smelling_up_0`. Each of these stopped
the game with `can't find cycle` before `d7b120fab`. The first attempt of this phase also
hit `point.angular_velocity is zero` (missing velocity lines, fixed in `59380e01a`) and
then hung after the fatal error on a worker thread (`2f6d89256`).

## Run 2026-10-11, pda phase (Linux)

    PASS pda_tab_eptMap active section: eptMap
    PASS pda_tab_eptTasks active section: eptTasks
    PASS pda_quit clean shutdown

The map tab works (Cordon level map, actor and task spots, zoom and filter buttons); the
older note that `pUIMapWnd` stays null is outdated. The screenshots showed two flaws,
both fixed: the tab bar kept the previous tab highlighted when a section was opened
directly (`e8c40c009`), and the UTF-8 string table overlays showed "AuftrÃ¤ge"
(`ff5c04dcf`). The plain `screenshot` console command did nothing before `1ef3b97b9`.

## Run 2026-10-12, PDA ports (Linux)

    menu/play/load: 11 PASS
    pda: 8 PASS (news step, all seven tabs, quit)
    dogs: 3 PASS

All seven PDA tabs were compared with screenshots of the original game. The tasks tab
(`4aebb0f2e`) and the map scroll bars (`375b7fa3a`) now match; the "new content" signs
light up for diary and encyclopedia after a new game (`53447475b`). The new news step
showed the whole `ui_iconsTotal` atlas before `b167b1fd3`, now the single icon.

The first dogs run of the day stopped with `valid_vertex_id(m_game_vertex_id)` in
`CStateMonsterSmartTerrainTaskGraphWalk<CAI_Flesh>::execute`: a dangling pointer to a
temporary Lua smart terrain task, fixed in `828679d85`. The crash depends on when Lua's
garbage collector runs, so one clean run is weak evidence; further dogs runs are planned.
