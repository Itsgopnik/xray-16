# SHoC-Testprotokoll: Setup, Ablauf & Log-Analyse

> Deutsche Anleitung zum systematischen Testen des Shadow-of-Chernobyl-Modus (Branch `soc_dev2`).
> Begleitend zu `doc/soc/pda.md` (PDA-Arbeitsstand) und Issue **#392**. Stand: 2026-09-25.

## 1. Test-Setup (fertig eingerichtet)

Die Testumgebung liegt **portabel im Projektordner** — Steam-Original bleibt unangetastet:

| Element | Ort | Zweck |
|---------|-----|-------|
| Engine-Build | `bin/x86_64/Release/xr_3da` (+ `.so`-Bibliotheken) | Build vom 24.09., Branch `soc_dev2` (PDA-Commit enthalten) |
| SoC-Archive | `bin/x86_64/Release/gamedata.db0 … dbd` | **Symlinks** auf die Steam-Installation (kein Kopieren nötig) |
| `fsgame.ltx` | `bin/x86_64/Release/fsgame.ltx` | SoC-Variante (aus dem Steam-Ordner, 1:1, dort am 24.09. im Einsatz) |
| CoP-Backup | `bin/x86_64/Release/fsgame.ltx.cop-bak` | die vorherige OpenXRay/CoP-Variante, zum Zurückwechseln |
| User-Daten | `bin/x86_64/Release/_appdata_/` | Logs, Saves, Shader-Cache (durch `$app_data_root$` isoliert) |
| GL-Shader | `bin/x86_64/Release/gamedata/shaders/gl` | **Symlink** auf `…/Stalker Call of Pripyat/gamedata/shaders/gl` — SoC-OS-Archive enthalten nur DX-Shader; die GLSL-Shader stammen aus dem CoP-Set und werden mit `USE_SHOC_RESOURCES` kompiliert. **Ohne diesen Link: `~ No shaders found for OpenGL` → FATAL `Can't setup renderer`** |

**Wichtig:** Das Setup muss aus `bin/x86_64/Release/` gestartet werden, damit `$fs_root$` dorthin auflöst und die Symlink-Archive gefunden werden (`fsgame.ltx` wird relativ zum Arbeitsverzeichnis gesucht).

## 2. Startbefehle

```bash
cd /home/itsgopnik/Dokumente/S.T.A.L.K.E.R/xray-16/bin/x86_64/Release

# Neues Spiel direkt, ohne Hauptmenü (Sofort-Test aus doc/soc/pda.md):
./xr_3da -nointro -shoc -start 'server(all/single/alife/new)' 'client(localhost)'

# ... oder klassisch über das Hauptmenü:
./xr_3da -nointro -shoc
```

Halte das Protokoll fest: Datum, Commit (`git log -n 1 --format=%h`), Startbefehl.

> Notiz: Alternativ/zusätzlich lässt sich der Modus persistent per `openxray.ltx` setzen (im Ordner `gamedata/config/` neben `fsgame.ltx` — dort sucht die Engine über `$game_config$`):
> ```ini
> ; gamedata/config/openxray.ltx
> [compatibility]
> game_mode = shoc
> ```
> Ohne Flag **und** ohne Datei läuft die Engine im CoP-Modus gegen SoC-Daten — genau das passierte versehentlich am 24.09. (s. nächster Abschnitt).

## 3. Was der letzte Lauf (24.09.) gelehrt hat

Der Lauf vom 24.09. (Log: Steam-Ordner `_appdata_/logs/openxray_itsgopnik.log`) startete **ohne `-shoc`**:

- `command line -nointro` → CoP-Modus auf SoC-Daten. Erklärt: CoP-UI-Texturen-Probiererei (`ui\ui_actor_loadgame_screen`), Upgrade-Fehler (`[upgrades_properties] does not exist` — SoC hat kein Upgrade-System), Shader-Versionskonflikte (r1-Shader-Set), unbekannte Konsolenbefehle aus SoC-`user.ltx`.
- **Absturz (FATAL)** beim Start eines neuen Spiels:
  `object_item_script.cpp:51` — `CObjectItemScript::server_object`: eine Lua-Server-Fabrik aus SoCs Spawn-Daten lieferte `nullptr` (Keine Exception, still) → Assertion unter `CALifeSpawnRegistry::load`. Der Stapel: new game → Spawn-Registry → Skript-Objekt-Wrapper.
- Drei maximale Erkenntnisse daraus: (1) **Immer `-shoc` mitgeben bzw. `openxray.ltx` anlegen.** (2) Der FATAL ist vermutlich modusabhängig — im CoP-Modus fehlen Teile der SoC-Skript-API. (3) Wenn er im `-shoc`-Lauf reproduzierbar bleibt, ist `object_item_script.cpp:51` + die zugehörige section die nächste Fix-Stelle.

## 3a. Gefunden und gefixt (25.09.2026): Archiv-Lookup war komplett kaputt

Der vermeintliche Modus-Fehler lag tiefer — die SoC-Archive waren **überhaupt nicht auffindbar**:

**Ursachenkette:**
1. Der frühe Scan mountet `.db*`/`.xdb*` aus dem **Binary-Ordner** (`_initialize` → `$app_root$`-Alias via `flScanAppRoot`, `bin/x86_64/Release/`).
2. Das geschieht **vor** `setup_fs_path()` — das Alias `$fs_root$` existiert noch nicht.
3. `CLocatorAPI::LoadArchive` konnte daher keine Basis ableiten → alle ~29.600 Archiv-Einträge registrierten unter **relativem** `gamedata\…`-Prefix.
4. Alle Lookups laufen über **absolute** Pfade (`$fs_root$` = CWD) → Archive-Dateien komplett unsichtbar (Konfigs, Skripte, Texturen, Shader …). Lose Dateien im `gamedata/`-Ordner funktionierten weiter — deshalb schien gestern Morgen manches zu gehen.

**Fix (17 Zeilen, `src/xrCore/LocatorAPI.cpp`, `LoadArchive`):** Fehlt `$fs_root$`, wird die Basis aus dem **Ordner der Archivdatei** (`A.path`) abgeleitet. Damit sind die Keys absolut — im Portabel-Layout (Binary-Ordner = CWD) identisch mit dem Lookup-Root. Der `Recurse`-Aufruf in `setup_fs_path()` (WIP aus dem PDA-Commit) bleibt unverändert und zusammen mit dem Fix regressionsgetestet.

**Ergebnis (Headless-Run, `SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1`):**
- `system.ltx` + alle Skripte laden aus den Archiven, `!`-Warnungen verschwinden weitgehend,
- GL-Renderer aktiv (`renderer_r3`), Voraussetzung: `gamedata/shaders/gl`-Symlink auf die CoP-GL-Shader-Set,
- **neues Spiel startet ins Level** (l01_escape): Hund-Schwärme spawnen, Slots-System aktiv, Spieler-Slots blockiert/aktiviert, **0 FATALs**.

**Debug-Methodik (überragbar wiederverwendbar):** Engine läuft headless über `SDL_VIDEODRIVER=offscreen`, Tests damit voll automatisierbar. Vermisst jemand Archiv-Dateien: temporär `Msg("Register[%d] [%s]",…)` in `CLocatorAPI::Register` aktivieren — zeigt die registrierten Keys direkt.

## 4. Durchklick-Checkliste für einen Lauf

Neues Spiel starten (Startbefehl oben), dann in dieser Reihenfolge prüfen (SVG = „sollte funktionieren“; Abweichungen notieren):

**A. Startup & Level (l01_escape)**
- [ ] Fenster/Renderer startet ohne FATAL (GL-Shader-Kache wird neu befüllt)
- [ ] Level lädt bis zum Spielereintritt (Sidorovich-Bunker)
- [ ] Himmel/Wetter/Licht wirkt korrekt (statische Sonne — SoC!)

**B. HUD & Basisspiel**
- [ ] Health-/Stamina-Anzeige (SoC: **keine Rüstungsleiste** — `UIHudStatesWnd`)
- [ ] Waffe ziehen, nachladen, schießen (Scope testen — `Weapon.cpp:536`, LoadScope im SoC-Modus ausgesetzt → nur Eisen-/Kim-Visier prüfen)
- [ ] Inventar öffnen (Slot-Verhalten `ShadowOfChernobylMode`: Standard in Slots, Gürtelbreite fix)
- [ ] Items aufnehmen/benutzen (3-s-Anzeige)
- [ ] NPC ansprechen (Dialogfenster) und Handel öffnen

**C. PDA (Schwerpunkt des soc_dev2-Stands — Details in `doc/soc/pda.md`)**
- [ ] Tab 0 Aufgaben: Liste, Filter, Doppelklick, Restzeit-Blinken, Karte/…
- [ ] Tab 1 Karte: Marker, Zoom, **(„ungetestet“ laut pda.md!)**
- [ ] Tab 2 Tagebuch: Journal/Nachrichten, gelesen/ungelesen
- [ ] Tab 3 Kontakte: Liste aktualisiert sich, Biografie rechts
- [ ] Tab 4 Rangliste: Top-20, eigener Eintrag
- [ ] Tab 5 Statistik: **(„ungetestet“ laut pda.md!)**
- [ ] Tab 6 Enzyklopädie: Gruppen, Artikeltext, gelesen

**D. Speichern/Laden**
- [ ] Quicksave/Quickload (nach dem Fix-Tipp aus pda.md: altes Save abwerten, nur Neuspawn testen)
- [ ] Savegame-Liste im Lade-Dialog zeigt SoC-Saves korrekt (beschwört `saved_game_wrapper.cpp`)

## 5. Log-Analyse (nach dem Lauf)

```bash
cd /home/itsgopnik/Dokumente/S.T.A.L.K.E.R/xray-16/bin/x86_64/Release

# FATALs und Assertionen
grep -n "FATAL\|assert" _appdata_/logs/openxray_itsgopnik.log

# Alle Warnungen (!-Zeilen) mit Zeilennummern
grep -n "^!" _appdata_/logs/openxray_itsgopnik.log

# Skript-/Lua-Fehler
grep -in "script\|lua" _appdata_/logs/openxray_itsgopnik.log

# Nur Fehlerzählung
grep -c "^!" _appdata_/logs/openxray_itsgopnik.log
```

Auswertung sortieren nach: (1) FATALs, (2) Errors vor jedem Warn-Block, (3) mehrfach auftretende Muster ( oft dieselbe Ursache). Ergebnisse nach jedem Lauf in #392 festhalten: Datum, Commit, Befehl, Beobachtungen, Log-Auszug.

## 6. Bekannte offene Baustellen im Code (Stand soc_dev2)

| Stelle | Status | Schweregrad |
|--------|--------|--------------|
| **GEFIXT (25.09.): Archiv-Lookup** (`LoadArchive`-Basis) | neue Spiele starten ohne lose Dateien; Session lief mit 0 FATALs, 146 Skripten, 120 Render-Targets | – Blocker behoben |
| **PDA-InfPortions fehlen** (`eptTasks`…`eptEncyclopedia`, `ui_talk_show`, je 1 Warnung) | UI erwartet Info-Portions, die SoC-Configs nicht definieren — im SoC-Modus bewusst nur Warnung (`InfoPortion.cpp:34`). Verhalten prüfen (Tab-Wechsel-Events) oder Aliasse anlegen | Gameplay/UI |
| **Level-Map-Texturen (Teil 1+2 GEFIXT 25.09.)** | `[global_map]` + `[level]`-Sektionen mit `texture`/`bound_rect`/`global_rect` existieren bereits in SoCs `game_maps_single.ltx` (via `game.ltx`-Include); die `map\\map_*`-Karten liegen in den Archiven. Fix a) `UIMap.cpp/Init_internal`: SHoC-Textur-Existenz-Fallback auf `ui_map_nopic`; Fix b) `CUICustomMap::Initialize`: SHoC-Prioritäts-Weiche — `[level]`-Sektion aus `game.ltx` vor `level.ltx [level_map]`. Ergebnis: 0 Warnungen (vorher 30× default + Fehltexturen), echte SoC-Globalkarte + Level-Kacheln laden. Hinweis: MP-Levels (`game_maps_mp.ltx`) sind analog strukturiert | UI/Karte — erledigt |
| **Hunde-Idle-Crash GEFIXT (25.09.)** — `dog.cpp/start_animation` | SoC-Motion-Sets fehlen einige CoP-Zusatz-Idles (`sit_idle_1`, ggf. `stand_threaten_0` …); `PlayCycle` assertionierte hart (FATAL in der Desktop-Session beim Hund-Sitz-Idle). Fix: `ID_Cycle_Safe`-Prüfung + posen-kompatibler Fallback (`sit_*→sit_idle_0`, sonst `stand_idle_1`; existiert der auch nicht: Zyklus überspringen). Bestätigung der nächsten Session vorbehalten (Log zeigt `~ SHoC: dog animation …`) | KI-Crash behoben |
| **Dog-Swarm-Anim-Warnungen** (`stand_drage_`, `stand_growl_walk_`, `stand_walk_smelling_` je 4×) | nur Warnungen aus dem Anim-Manager (AddAnim-Präfixe); kein Blocker — Feintuning: SoC-präfixe ergänzen oder überspringen | KI/Kosmetik (offen, harmlos) |
| **Wasser/Tracer-Texturen (GEFIXT 25.09.)** | Drei-Lösungen: a) `rain_patch_normal.ps` per `#ifdef USE_SHOC_RESOURCES` auf `Texture2D` mit `water\\water_normal` (SoC hat keine Volume-Bump-Textur) — GL-Shader liegt im Repo (`res/gamedata/shaders/gl`, 444 Dateien — symlink im Test-Setup zeigt dorthin); b) Blender (`glRainBlender.cpp`/`dx11RainBlender.cpp`) binden im SHoC-Mode `water_normal`; c) `Tracer.cpp` fällt auf SoCs `fx\\fx_tracer` zurück. Verifiziert: 0 Fehltexturen, Tracer-Fallback greift (Log: `~ SHoC: tracer texture …`) | Renderer — erledigt |
| **r1-Blender-Konflikte** (`Version conflict in shader 'models\\x*'`, `default`, `shadow_world` — ~10 Meldungen) | legacy r1-Blender-Warnungen; Glas/Monolith-Deko visuell prüfen; größeres Shader-Thema (separater Task) | Renderer (offen, kosmetisch) |
| `Weapon.cpp:536` Scope-Load ausgesetzt (XXX, temporär) | Visier-UI fehlt im SoC-Modus | Gameplay |
| `ShootingObject.cpp` Partikelpositionen (3× XXX) | Effekte ggf. deplaciert | Kosmetik/Gameplay |
| `actor_communication.cpp` Enzyklopädie-UI auskommentiert | SoC-Artikel-Anzeige fehlt evtl. | UI |
| Inventar-Szenarien (`Inventory.cpp`-Weichen) | Slot/belt-Verhalten beobachten | Gameplay |
| Wetter-Aliase `$game_weathers$`/`$game_weather_effects$` fehlen in der SoC-`fsgame.ltx` | Wetter ggf. unvollständig (Beobachtung …) | Umgebung |
| **Options-Menü-Crash GEFIXT (25.09., spät)** `ScriptXMLInit.cpp/InitTab` | Socs `ui\ui_mm_opt.xml` definiert Tabs ohne `id`-Attribut (SoC-Format); `CScriptXmlInit::InitTab` rief `InitTabControl` ohne `defaultIdsAllowed` → FATAL („Tab control tab doesn't have 'id' assigned“ beim Klick auf Optionen im Hauptmenü). Fix: `ShadowOfChernobylMode` als `defaultIdsAllowed` übergeben (gleiche Lösung wie `UIPdaWnd:194`). Begleiterscheinung: `ui_ingame2_opt_slider_*`-CoP-Texturen fehlen (Dummy-Fallback, nur Warnung). Verifikation: Headless-Regressionslauf ohne FATAL; Options-Öffnung Desktop-Test | UI — erledigt |
| **Options-Slider-Darstellung GEFIXT (25.09., spät)** | `UITrackBar.cpp` hardcodiert CoP-Atlas-IDs (`ui_inGame2_opt_slider_*`), SHoC-Fallback auf Blank-IDs (`ui_slider_e/d`), die SoC nicht kennt (nur `ui_slider_e_back/_b/_e` in `ui_common.xml`). Fix a) `ui_base.cpp`: SoCs `ui_common.xml`-Atlas (Slider, Scroll-Bars, Tab-Buttons, Checkboxes) via `ParseShTexInfo("ui", "ui_common.xml")` registriert; Fix b) `UITrackBar.cpp`: SHoC-Zweig nutzt `ui_slider_e_back`/`ui_slider_d_back` für die Slider-Linie. Verifikation: 0 Slider-Warnungen, kein FATAL, kein Regress | UI — erledigt |

### Ergebnisse der ersten echten Desktop-Session (25.09., nach dem FS-Fix)

- Programmstart über `start_shoc.sh` (im SHoC-Modus, bestätigt: `command line -nointro -shoc`), PDA geöffnet, sauber beendet (`KERNEL:QUIT`, `user.ltx` gespeichert).
- **3.088 Log-Zeilen, 0 FATAL, 96 Warnungen, 146 Skripte, 120 Render-Targets/Shader** — sehr sauberer Stand.
- `Cannot open script file [user.ltx]` (2×) ist die Konsole, die die INI als „config-script“ ausführt — irreführende Meldung, funktioniert („Config-file … saved successfully“), harmlos.

### Options-UI Darstellungsfehler — Screenshot-A/B-Analyse (26.09.)

Vier Screenshots (je Tab: Video/Sound/Game/Controls) wurden per pixel-basierter Bildanalyse (Sub-Agent) VORHER/NACHHER verglichen:

**GEFIXT durch `stretch_default`-Test (UIXmlInitBase.cpp:853, SoC 0→1) + Slider-Fallback-Revert:**
- ✅ P1 Slider-Tracks: VOLL-visibleitbar (6 Slider über 3 Tabs, ~480px breit, Endkappen, Knopf sauber)
- ✅ Keine Regressions irgendwo (Buttons/Checkboxen/Scrollbars bitweise unverändert)

**VERBLEIBEND (kosmetisch, pixel-verifiziert):**
1. **Checkboxen deformiert**: „Full Screen“ 39×17 statt SoC-Original 32×23 — XML-Fenstergröße stretcht die Atlas-Textur ins Falsche (Verhältnis ~2.3:1 vs. ~1.4:1 Original). „Advanced“: nur vertikaler orangefarbener Strich (Checkbox in Breite gequetscht).
2. **Buttons Use/Cancel**: Strichel-Nahtmuster (~20–30px-Periode = FrameLine-Kachelung), überstehende Fragmente rechts + im Zwischenraum, ungleiche Breiten (247 vs. 265px).
3. **Scrollbar**: Dialog-Scrollbar nur ~6px breit statt ~14–20px (gequetscht, neutralgrau); innerer Daumen (Controls-Tab) vertikal überdehnt, Textur kachelt.
4. **Geister-Block**: Heller versetzter Text-Block hängt unterm aktiven Tab in den Zeilenbereich (in jedem Tab).
5. Rohdaten-Details (Wertboxen unsichtbar, Rahmen-Lücken, Game-Tab-Gradient-Balken)

**Offene Punkte / Hinweise:**
- `stretch_default` SoC=1 (A/B geprüft, kein Regress auf Options-UI) — PDA-Screenshots fehlen für den finalen Regression-Check. Ursprung des Wertes 0: PDA-Commit `d8f5e7048` (Zeile „stretch_texture-Default: 0 statt 1 für SoC/CS")
- Realistisch SINGLE-CHECKBOX-Ursache: SoC-XML maße vs CoP-Texture-Mapping (Widget-Größe aus XML bestimmenDrawing — null Ursache vermutlich in CUI3tButton/InitCheckButton-Größe (CUI_IB_Static zeichnet in Fenstergröße statt nativer Atlas-Größe 32×23)

**Nächste Schritte (Priorität):**
1. Checkbox-Kwadratur (CUI3tButton bei frameline_mode=false → SetWndSize auf Texture-Maße stützen ODER XML-Zielmaße interpretieren) — Screenshot-A/B verifizieren
2. Scrollbar (CUIScrollBar: ui_scroll_* Registrierung prüfen — die ids existieren im ui_common-Atlas)
3. Button-Nahtungen (CUIFrameLine(/Button) mit stretch_default=1 evtl. global gelöst? — prüfen, da Buttons bitweise unverändert blieben — vermutlich frameline_mode-Texturpfad!)
4. Geister-Block (Tab-Label zweizeilig/expansion?)

### Savegame-Load gefixt (25.09., späte Session): luabind-Error-Callback war nie gesetzt

**Symptom:** Laden eines Spielstands (z.B. Quick `itsgopnik - quicksave.sav`) crashte beim Actor-Spawn mit `std::unexpected()`/`Unexpected application termination`.

**Ursachenkette (dreifach verschachtelt):**
1. Original-1.0006-Skript-Bug: `task_manager.script:222` (=Patch-Version `dbc`!) indexiert beim Restore eines gespeicherten `defend_lager`-Tasks `defend_object` ohne nil-Check, wenn das Ziel (id 65535/offline despawnt) nicht mehr existiert. Exakt das von `doc/soc/pda.md` prophezeite „Savegame-Load scheitert im Aufgabensystem“.
2. Ein uralter Engine-Konfigurationsfehler: `CScriptEngine::setup_callbacks()` setzte `luabind::set_error_callback(...)` nur unter `#if !XRAY_EXCEPTIONS` — im Release-Build (`XRAY_EXCEPTIONS=1`) also **nie**. Da luabind mit `LUABIND_NO_EXCEPTIONS` gebaut wird, lief jeder Skriptfehler in `call_error()` → Callback NULL → `assert + std::terminate()`. (Beträf auch CoP — bemaskt durch den FATAL-Dialog in `lua_pcall_failed`.)
3. `lua_error` warf `const char*` (Engine-seitige Exception-Semantik) aus einem luabind-NO_EXCEPTIONS-Kontext → `std::unexpected()` → terminate.

**Fixes (alle in `src/xrScriptEngine/script_engine.cpp`):**
1. `setup_callbacks()`: Bedingung korrigiert auf `#if defined(LUABIND_NO_EXCEPTIONS)` — der Callback wird jetzt gesetzt.
2. `lua_pcall_failed` (msg-handler): SHoC-Zweig loggt die Meldung (`[pcall_failed]`), dupliziert das Fehlerobjekt und kehrt ohne FATAL zurück.
3. `lua_error` (error-callback): im `LUABIND_NO_EXCEPTIONS`-Build nie werfen — SHoC-Modus loggt (`[error_callback]`) und kehrt zurück; Non-SHoC-STände terminieren explizit mit klarer Meldung statt `std::unexpected`.

**Verifikation (Headless, `start server('itsgopnik - quicksave/single/alife/load')`):** Save lädt (0,96 s), beide Handler feuern genau einmal, **0 FATAL`, 12 gespeicherte Hund-Rudel weiter aktiv**, Spiel läuft. Erwartete Restriktion: der verteidigungs-Task im geladenen Spiel hat kein Ziel mehr (Original-Skript-Bug) — Task ggf. abbrechbar/ignorierbar.

## 7. Wiedereinstieg / Tipps

- Der Modus ist global — Moduswechsel nur per Neustart (Flag oder `openxray.ltx`).
- Physik-Abhängigkeiten: Nach einem Moduswechsel Cache in `_appdata_/shaders_cache_oxr/` löschen, wenn Shader-Kompilierung seltsam wirkt.
- Für Regressionstests: ohne `-shoc` läuft dasselbe Setup als (unvollständiger) CoP-Modus; für echte CoP-Tests lieber das CoP-Setup wiederherstellen (`fsgame.ltx.cop-bak` einsetzen) — die SoC-Daten gehören nicht in CoP-Läufe.

---
*Dokumentationsbasis: Log-Analyse (24.09., CoP-Modus-Lauf), `doc/soc/pda.md`, `x_ray.cpp` (InitSettings), Source-Greps der SoC-Weichen (vgl. `docs/09-shoc-anpassungen.md`), Setup-Experimente vom 25.09.*
