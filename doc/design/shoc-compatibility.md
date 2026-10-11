# Shadow of Chernobyl compatibility layer

Design note for running S.T.A.L.K.E.R.: Shadow of Chernobyl 1.0006 on OpenXRay
(issue [#392](https://github.com/OpenXRay/xray-16/issues/392)). It lists the format and content
differences found so far and how the engine handles them. Status of individual
features is tracked in the issue itself.

## Principles

1. **Original data is never modified.** The engine reads the SoC `.db` archives as they
   are. Fixes for bugs in GSC's own scripts go into an optional gamedata overlay, never
   into the engine.
2. **SoC behaviour is isolated.** Every SoC-specific branch checks
   `ShadowOfChernobylMode` (or, preferably, the data itself). CoP, CS and CoC code
   paths must not change.
3. **Prefer data-driven checks over game checks.** If the question is "does this
   texture/section/tutorial exist?", ask the file system or the config instead of the
   game mode (example: the rain blenders fall back to `water_normal` only when
   `water_SBumpVolume` is missing).
4. **Don't gate on `ClearSkyMode` without verifying CS data.** CS has features SoC lacks
   (upgrades, for example). Wrong `SoC || CS` checks are an easy way to break CS.
5. **Tolerate, but report.** Where SoC data legitimately differs from what CoP code
   asserts, the assert becomes a warning that is logged once (per session or per
   object/visual), not on every frame.

## Game type selection

The global flags `ShadowOfChernobylMode`, `ClearSkyMode` and `CallOfPripyatMode`
(`src/xrEngine/defines.h`) are the source of truth: exactly one is set, or none in
`unlock` mode (CoP without restrictions, also used for CoP-based mods like CoC).
`EGameType GetGameType()` gives a read-only enum view for code that needs to switch on
the game. The flags are deliberately not replaced: they are used in ~50 files and by
upstream code.

Selection order in `InitSettings()` (`src/xrEngine/x_ray.cpp`):

1. Command line: `-shoc`/`-soc`, `-cs`, `-cop`, `-unlock_game_mode`
2. `openxray.ltx`, `[compatibility] game_mode`: `auto` (default), `shoc`/`soc`, `cs`,
   `cop`, `unlock`. Unknown values are logged with `!`.
3. `auto`: `shoc` if any mounted archive is an encrypted SoC archive
   (`CLocatorAPI::has_encrypted_archives()`), `cop` otherwise. CS, CoP and CoC data
   can't be told apart reliably from the file system, so CS still needs `-cs` or
   `game_mode = cs`.

The selected mode is always logged (`* Game mode: shoc`).

## Format and content differences

| Area | SoC 1.0006 | CoP (engine baseline) | Handling |
|---|---|---|---|
| Archives | `gamedata.db0`–`dbd`, encrypted (WW and RU keys), no ini header | `.db`/`.xdb` with ini header | `LocatorAPI` decrypts archives without header; also the `auto` detection signal |
| FS layout | `config\` | `configs\` | Described by the game's own `fsgame.ltx` |
| Weather | ShoC-style weather sections | CoP sections | Detected in `Environment_misc.cpp` |
| Texture descriptions | Old `textures.ltx` format | New format | Supported upstream |
| `game.graph`, `level.gct` | Older versions | Current versions | Versioned readers upstream |
| Actor script binding | `bind_stalker.actor_init` | `bind_actor.init` | Read from `system.ltx` `[actor] script_binding` (engine and smoke test) |
| Info portions | Declared in XML | Not declared at all | SoC-only check before forwarding engine UI info portions (`SendInfoToActor`) |
| Inventory slots | No artefact slot, `slots_count` differs from scanned slots | Artefact slot 11 | Artefact key is ignored when the slot doesn't exist; mismatch warning logged once |
| HUD animations | `anim_*` names | `anm_*` names | Legacy fallback; missing-animation warnings suppressed in SoC |
| Monster animations | Some motion variants missing | Complete sets | Missing variants are removed from the set, logged once per visual |
| Dog group state animations | 9 of 17 CoP motions missing in all dog visuals (smelling, digging, howling, growling, shaking, 2 sit idles) | Present | Per-index fallback to the closest existing motion (`CAI_Dog::resolve_current_animation`); before, a resting pack stopped the game with `can't find cycle` |
| Monster velocities | No `Velocity_WalkSmelling`, `Velocity_WalkGrowl`, `Velocity_RunAttack` | Defined | Fall back to `Velocity_WalkFwdNormal`/`Velocity_RunFwdNormal`; zero velocities broke path building |
| Upgrades | None | Since CS | Upgrade manager skipped in SoC |
| `game_loaded` tutorial | Missing | Present | Load screen ends directly in SoC |
| Tracer texture | `fx\fx_tracer` | `fx\fx_tracer_weapons` | SoC default changed (still overridable in `system.ltx`) |
| Water textures | `water_normal` only | `water_SBumpVolume`, `water_flowing_nmap` | Data-driven fallback in the rain blenders |
| Month names | No `month_*` string keys | Present | Numeric month when the key isn't translated |
| Level maps | `game_maps.ltx` `[def_map]`, schematic rects | Per-level `[level_map]` | CoP consistency checks skipped in SoC |
| UI textures | Per-part DDS files, old XML layouts | Atlas ids | Alias table in `UICore::ReadTextureInfo`; tolerant XML parsing (`maingame.xml` without `<main>`, unknown color names) |
| PDA | Tasks/diary/contacts/ranking tabs | Different PDA | Separate `CUISHoC*Wnd` windows in SoC mode |
| String tables | Single-byte text (windows-1252 bytes, header says windows-1251) | Same | The engine doesn't convert encodings: our overlays must be stored as windows-1252, not UTF-8 |
| Scripts | Non-normalized direction vectors, duplicate `give_task` ids, no task type | Strict | Normalize/warn instead of assert; default task type storyline |

### Script API

Method: the SoC 1.0006 scripts and configs were dumped through the engine
(`misc/tests/tools/dump_scripts.lua`) and compared with the engine's `-dump_bindings`
output (`misc/tests/tools/find_missing_exports.py`). The 50 candidates were then checked
for liveness: is the module a registered logic scheme (`load_scheme`) or referenced from
a config or a live script?

| Name | Used by | Live? | Handling |
|---|---|---|---|
| `game_object:nearest_accessible_position()` | `xr_attendant` (follower scheme; Sarcofag, Selo, Dark Valley escort, Cit jail) | Yes | Shim (SoC only) |
| `level.actor()` | `xr_rest` (`rest` scheme), `_g.get_actor_id` | Unknown: depends on gulag type `rest` in the spawn data | Shim (SoC only), call sites logged |
| `cse_anomalous_zone.update`, `cse_zone_visual.update` | `se_zones` periodic artefact respawn | No, also not in vanilla | None. The original 1.0006 `xrGame.dll` shows (RTTI) that anomalous zones aren't `CSE_ALifeSchedulable` and that `cse_anomalous_zone` exports no `update` callback, so `se_zone_anom:update()` never ran in the original game either. Vanilla only places artefacts in anomalies when a new game starts (`on_spawn`), which this engine does too (183 artefacts on a new game) |
| `cse_anomalous_zone:spawn_artefacts()` | `se_zones` (only from the dead `update` path) | No in vanilla; mods may call it | Exported in SoC mode, as in the original DLL; ignored with a log line for online zones |
| `actor_stats.add_to_ranking`, `remove_from_ranking` | `bind_stalker`, `se_stalker`, `xr_motivator` | Guarded with `~= nil` | Not needed: `CUISHoCRankingWnd` builds the list from the ALife registry |
| `CHelicopter.eIdleState` etc., `CHelicopter:SetState` | `xr_heli` via `escape_heli` | No (`escape_heli` is never loaded; the live helicopter uses `heli_move`) | None |
| `level.object()` | `_g.debug_get_level_object`, `script_object`, unregistered schemes | No | None |
| `actor_sleep`, `clear_callback`, `get_fsm_state`, `send_pda_message`, `set_node_evaluator`, `set_path_evaluator` | `xr_effects.force_actor_sleep`, unregistered schemes | No | None |
| Others (`game_last*`, `lua_help`, `test_*`, MP UI classes) | Test, debug and multiplayer scripts | No | None |

Shims live in `src/xrGame/script_shoc_compat.cpp`, are registered only in SoC mode, and
log every distinct call site once: `~ [shoc-shim] level.actor called from ...:line`.

### Saves

Saves written by OpenXRay in SoC mode load correctly (in-process and cold, see the
smoke test). Loading saves made by the original 1.0006 executable has not been tested
and is not a goal at the moment.

## Testing

`misc/tests/smoke/run_smoke.sh` runs the repeatable smoke test (boot to main menu, new
game, kill one enemy, save, load in-process and from the command line, quit). It drives
the game through the `-test_script` hook, which exists only in non-`MASTER_GOLD` builds.
Results are recorded in [shoc-smoke-test-log.md](shoc-smoke-test-log.md).

## Open points

- Static analysis can't see calls built at runtime (`loadstring`, string-assembled
  names) or uses from spawn data; runtime shim logs cover only the shimmed functions.
- The R4 renderer doesn't start with SoC data (#487).
- No CS, CoP or CoC regression run has been done with the smoke test yet.
- Windows has not been tested.
