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
| Upgrades | None | Since CS | Upgrade manager skipped in SoC |
| `game_loaded` tutorial | Missing | Present | Load screen ends directly in SoC |
| Tracer texture | `fx\fx_tracer` | `fx\fx_tracer_weapons` | SoC default changed (still overridable in `system.ltx`) |
| Water textures | `water_normal` only | `water_SBumpVolume`, `water_flowing_nmap` | Data-driven fallback in the rain blenders |
| Month names | No `month_*` string keys | Present | Numeric month when the key isn't translated |
| Level maps | `game_maps.ltx` `[def_map]`, schematic rects | Per-level `[level_map]` | CoP consistency checks skipped in SoC |
| UI textures | Per-part DDS files, old XML layouts | Atlas ids | Alias table in `UICore::ReadTextureInfo`; tolerant XML parsing (`maingame.xml` without `<main>`, unknown color names) |
| PDA | Tasks/diary/contacts/ranking tabs | Different PDA | Separate `CUISHoC*Wnd` windows in SoC mode |
| Scripts | Non-normalized direction vectors, duplicate `give_task` ids, no task type | Strict | Normalize/warn instead of assert; default task type storyline |

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

- Script API gaps (issue #392: "Return deleted script export") have no shim logging yet.
- The R4 renderer doesn't start with SoC data (#487).
- No CS, CoP or CoC regression run has been done with the smoke test yet.
- Windows has not been tested.
