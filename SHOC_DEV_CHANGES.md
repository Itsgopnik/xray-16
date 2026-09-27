# SHoC-Kompatibilität: Änderungsdokumentation

> Session vom 24.09.2026 · Branch `soc_dev2` · Build: Debug (GCC 16, CachyOS, native Linux)
> Ziel: SoC-Aufträge im PDA reparieren; dabei entstandene Engine-/Stabilitätsfixes dokumentieren.

---

## 1. Dev-Umgebung (nicht im Repo)

Pfad: `~/dev/shoc-dev/` — getrennt von der Steam-Installation (nur Symlinks auf die `.db*`-Archive).

```
shoc-dev/
├── gamedata.db0 … dbd        ← Symlinks auf Steam-Installation (read-only)
├── fsgame.ltx                ← SoC-FS-Layout (config\ statt configs\)
├── run.sh                    ← Startscript (Debug-Engine + -soc -fsltx)
├── exceptions.gdb            ← gdb-Exception-Monitor (loggt C++-Throws als Backtrace, ohne Anhalten)
├── _appdata_/                ← eigene Saves/Logs/user.ltx (getrennt von Steam)
├── shoc_db_unpack.py         ← Python-Entpacker für die SoC-Archive (s. Werkzeuge)
├── shoc_data/                ← extrahierte Patch-Dateien (aus dbc)
└── extract/                  ← per `dump_config_file` extrahierte Original-XMLs
```

Overlay `gamedata/` (gewinnt per Register-Last-Write-Wins über die Archive):
- `config/openxray.ltx` — `game_mode = shoc` (persistiert zusätzlich als `ui_style ui_style_shoc` in user.ltx)
- `config/text/eng|string_table_z_shoc_overlay.xml` — CoP-UI-Keys, die in SoC fehlen (PDA-Texte)
- `config/ui/styles/ui_style_shoc/` — deployter Style + `scroll_bar.xml` (Profile `default`/`pda`)
- `gamedata/scripts/task_manager.script` — **GSC-Bugfix** (s. Abschnitt 3.2)

---

## 2. Engine-Codefixes (Upstream-Kandidaten)

Alle Fixes sind SoC-tolerant bzw. generisch. Die Reihenfolge folgt dem Fund-Schweregrad.

### 2.1 `src/xrCore/xrMemory.cpp` — **kritischer Linux-Blocker (Allocator)**
**Problem:** Der globale `operator new(size_t, std::align_val_t)` im `USE_PURE_ALLOC`-Zweig ignorierte das Alignment (`malloc(size)`) und lieferte 16-Byte-Pointer. Bibliotheken mit über-alignment (z. B. OpenAL via `_ZnwmSt11align_val_t`) machen darauf aligned AVX2-Stores (`vmovdqa`) → **SIGSEGV vor `main()`** (ld.so-Init-Kette, kein Log, kein Core unter systemd).
**Fix:** `xr_aligned_malloc`/`xr_aligned_free` für alle aligned-Varianten (Paarung mit verstecktem Original-Pointer, wie MSVC `_aligned_malloc`). Gleichzeitig Bug im `USE_XR_ALIGNED_MALLOC`-Zweig behoben (`mi_malloc_aligned` → `xr_aligned_malloc`, Referenzen auf nicht existierendes mimalloc). Beide Zweige verwenden `xr_aligned_realloc` für realloc.

### 2.2 `src/xrCore/Memory/xrMemory_align.cpp` + `src/xrCore/CMakeLists.txt`
**Problem:** `xrMemory_align.cpp` war **aus dem Build auskommentiert** (CMakeLists L344f) — die `xr_aligned_*`-Symbole existierten nicht, POSIX-nichtkompatible Aufrufe (`_msize`, `_expand`) vorhanden.
**Fix:** Datei reaktiviert; `_msize` → `malloc_usable_size` via Makro (`xr_aligned_msize_impl`), `_expand`-Pfad auf Windows beschränkt (POSIX-Fallback: immer neu allozieren + memmove).
**Hinweis:** Der `USE_XR_ALIGNED_MALLOC`-Branch war durch den mimalloc-Bug nie nutzbar; der `USE_PURE_ALLOC`-Fix (2.1) behebt den Crash.

### 2.3 `src/xrEngine/Device_mode.cpp` — Fatal-Dialog-Ketten-Crash
**Problem:** `CRenderDevice::OnErrorDialog` → `UpdateWindowProps` → `ImGui::GetIO()` mit **bereits zerstörtem** ImGui-Context (z. B. Fatal während Shutdown) → asserts → SIGABRT — aus jedem Fehler wurden zwei.
**Fix:** Guard `if (!ImGui::GetCurrentContext()) return;` vor `GetIO()`.

### 2.4 `src/xrGame/stalker_movement_params_inline.h` — KI-Assert
**Problem:** `VERIFY(fsimilar(direction->magnitude(), 1.f))` — SoC-Scripts übergeben nicht-normalisierte Richtungsvektoren; Original-2007-Engine tolerierte das, CoP-Code asserted → Debug-Absturz bei NPC-KI-Script-Aufrufen.
**Fix:** Statt Assert: normalisieren; degenerierte Vektoren als „keine Richtung" behandeln (wie der Null-Fall `flt_max`-Satz).

### 2.5 `src/xrGame/ai/monsters/control_animation_base.cpp` — Monster-Spawn-Fatal
**Problem:** `VERIFY2(count != 0)` in `UpdateAnimCount` — SoC-Animsätze (z. B. `stand_drage_`) verfügen nicht über alle Motion-Varianten → Assert im `net_Spawn` tötete jeden Spielstart mit Hunden.
**Fix:** Assert entfernt — die Funktion hat ohnehin eine funktionierende Cleanup-Logik für fehlende Animationen; nur die Meldung bleibt.

### 2.6 `src/xrUICore/ScrollView/UIScrollView.cpp` — **PDA-Blocker (m_pad)**
**Problem:** `CUIScrollView::Clear()` dereferenziert `m_pad` ohne Initialisierungskontext — `m_pad` entsteht erst in `InitScrollView()`. PDA-Öffnen → `FillList()` → `Clear()` → **SIGSEGV bei jedem PDA-Öffnen** (Standard-Tab = Aufgaben).
**Fix:** `if (!m_pad) return;`-Guard. Zusätzlich rufen `UISHoCTaskWnd`/`UISHoCDiaryWnd` jetzt `InitScrollView()` nach `SetWndSize(...)` (erzeugt `m_pad` + Scrollbar; Header der Methode dokumentiert genau das).

### 2.7 `src/xrGame/GametaskManager.cpp` — give_task-Toleranz, Task-Typ-Default, Diagnostik

**Fix A (`give_task`-Toleranz):** Das Original enthielt `VERIFY2(0, "task already inprocess")`, das **unabhängig von `bCheckExisting`** crashte (das `bCheckExisting &&` in der Bedingung war auskommentiert). SoC-Scripts rufen `db.actor:give_task(task, time, false)` mit wiederverwendeten Task-IDs (gleicher Aufgabentyp mehrfach). Jetzt: Warnlog + `xr_delete(t)` (behebt zugleich ein Adopt-Leak, da Lua `adopt<2>` den Besitz übergibt) + NULL-Rückgabe statt Fatal.

**Fix B (Task-Typ-Default):** SoC-Scripts setzen nie `set_task_type`; neue Tasks haben `m_task_type == eTaskTypeDummy` (`u16(-1)`, im Log sichtbar als `type=65535`). Das CoP-`CUITaskWnd` sortiert nach Storyline/Secondary — ein Dummy-Typ landet in **keinem** Bucket, d. h. die Aufgaben wären unsichtbar. Jetzt: bei `eTaskTypeDummy`/`eTaskTypeCount` wird auf `eTaskTypeStoryline` gesetzt, mit `Msg("[SHOC-TASK] …​ type unset -> defaulted")`.

**Fix C (Diagnostik):** `[SHOC-TASK]`-Trace-Logs beim `give_task`-Empfang (ID, Priorität, Typ, bCheckExisting) und im PDA-TaskWnd-`FillList` (Task-Anzahl, Filter-Status, übersprungene Task-States) für das Live-Monitoring der Quest-Pipeline.

### 2.8 `src/xrGame/ui/UIPdaWnd.cpp`
- `Show_ContactsWnd()`: `if (true)`-Platzhalter → wirkliches `pUISHoCContactsWnd`-member geprüft.
- **SHoC-Mode nutzt jetzt das CoP-`CUITaskWnd`** als „eptTasks"-Dialog (mit eingebetteter Mini-Map, Fokus-Reglern, Zoom) vom vorher-entwickelten Style `pda_tasks.xml`; das eigene `CUISHoCTaskWnd` bleibt als Fallback erzeugt und verfügbar.
- **Programmatischer Hint-Fallback:** Original-SoC-XML kennt keine `hint_wnd`-Node (pda.xml); CoP-Fenster (Karten-Navigation) verlangen aber ein Hint-Objekt (`VERIFY`-Kette in `UIMapWnd2.cpp`). Ohne diese Fallbacks crashte der PDA-Bau. Fallback: erzeugt bei fehlender Node intern ein einfaches Default-Hint (FrameWindow + Statik). Ersetzt den früheren Ansatz, `hint_wnd` als Node in die Style-XML zu schreiben (nach Drop-in-Prinzip revertiert).

### 2.9 `src/xrUICore/XML/UIXmlInitBase.cpp` — Farb-Definitions-Fallback

**Problem:** `VERIFY(GetColorDefs()->find(clr_def) != end())` tötet jeden `color="…"`-Token, der nicht in `color_defs.xml` definiert ist. Ausgelöst durch CoP/CS-Style-Dateien mit CS-Namen-Tokens (`ui_9`, `ui_1`…), die in der Original-SoC-Definition nicht vorkommen (dort: `UI_orange`, `ui_clr_1`…). Betroffen waren Inventar/ActorMenu-Bau (Fatal beim Levelstart!) und der PDA-Bau (pda_tasks.xml-Verweise).

**Wichtiges Analyse-Ergebnis:** Die Original-SoC-`color_defs.xml` (aus den Archiven extrahiert) hat **dasselbe XML-Schema** wie die CS-Variante — die Engine lädt und parst sie korrekt. Die Crashes kamen ausschließlich von CS-Tokens in OpenXRays eigenen Style-Dateien, nicht von den Original-Ressourcen.
**Fix (Engine):** Fallback auf `def_clr` (statt Fatal) + `Msg("! [compat] color definition [%s] not found …")` — Drop-in-fähig, fängt künftig auch fremde Styles/Mods ab.
**Fix (Ressource):** OpenXRay-eigene Style-Dateien (`res/.../ui_style_shoc/pda_tasks.xml`) tragen jetzt RGB-Literale statt Tokens ( Repo-sync).

### 2.10 `src/xrUICore/ui_base.cpp` — Scrollbar-Textur-Mappings (`UICore::ReadTextureInfo`)
**Problem:** Standard-Scrollbars (CUIListBox ohne explizites `scroll_profile`, PDA-Profile) referenzieren CoC/CoP-Textur-IDs (`ui_scroll_box_back`, `ui_scroll_back_b`, `ui_scroll_pda_box_e_*`, `ui_scroll_pda_back_hor_*`, …) welche SoC-Archive nicht enthalten → 208 „Can't find texture"-Warnungen, leere/schwarze Balken.
**Fix:** Mapping-Tabelle um fehlende IDs erweitert (→ vorhandene SoC-DDS `ui_scb_*`). Zusätzlich deployt das Style-Overlay die fehlende Scrollbar-Profile-Datei `scroll_bar.xml` (Profile `default` + `pda`, aus dem CS-Style übernommen, da SoC-Stile im Standard-Bundle keine profile trugen).

### 2.11 `src/xrGame/ui/UISHoCTaskWnd.cpp/.h` — V2/V3 des Aufgaben-Fensters
- Initial noch mit hardcoded Fallback-Layout (V1, `m_pad`-Fix); V3: brauche jetzt die echte XML-Hierarchie (`main_wnd` → `left_frame` → `left_frame_header` → list / `primary_or_all` / `filter_tab:button` …) mit Handler — Original-XML ist parent-relativ, die ersten Versuche (V2) hatten die Knoten flach auf das Root-Fenster gelegt → deshalb waren Darstellungen „an den falschen Ecken".
- FrameLine-Header mit `:title`-Kindverarbeitung via `CUIXmlInitBase::InitFrameLine` (liest `:title` automatisch — selbes Reparaturprinzip wie 2.4 des Vorentwickler-Styles).
- Farb-Filter (state=InProgress|Completed|Fail — enum renumber vs. CS: InProgress=1, Completed=2, Fail=0) mit UI-Aktionen + `[SHOC-TASK]`-Trace-Zeilen; „show only general tasks"-Checkbox filtert auf Aktiven Task.
- u32-Sentinel in SoC-`r_u32()`-Wert `4294967295` in `task_manager.script`-Guard (s. 3.2) direkt verbaut.

---

## 3. SoC-eigene Bugfixes (per Gamedata-Overlay, nicht im Upstream-Repo)

### 3.1 Deutsches Stringtable-Overlay
`res/.../text/ger/string_table_z_shoc_overlay.xml` — deutsche Pendants der CoP-UI-Keys (in `text/ger/` im Repo inkludiert — unsere Eigenressource wie die GLSL-Shader).
- Neue Datei (`??` in git status) — wird parallel zur eng-Version ausgerollt.

### 3.2 `task_manager.script` — 2006-GSC-Original-Bugfix
**Problem (SoC-Original-Bug):** `load()`-Deserialisierung stürzt im Savegame-Wiedereinlesen ab: `alife():object(selected_target)` mit `selected_target==nil` (nie gewählte `defend_lager`-Vorlagen sind im Save serialisiert) — nil-Methode-Call. Der Fehler wird in der Original-Engine abgefangen (warnung), aber beim OpenXRay-Fill lässt es die Deserialisierung MIDWAY abbrechen → inkonsistenter Spielzustand → ISpatial-Ghost/Spieler-Duplikat → nachfolgende Spikes + Crash beim Speichern/Laden (`_node_destroy`-Assertion). Auch betroffen: mehrere `alife():object()`-Bindungen (`invalid id[65535]` bei `u32`-Wraparound 4294967295).
**Fix (Overlay-Kopie):** Einzeiler-Guards: `selected_target ~= nil and selected_target ~= 4294967295` → kein `alife():object(...)`-Aufruf und kein Crash beim Laden.

**Anmerkung:** Die Reparatur muss script-seitig erfolgen, da der Fehler im GSC-Skript selbst liegt (die Engine-API verhält sich spezifikationskonform; der Schaden entsteht durch den abgebrochenen Deserialisierungs-Stream und den inkonsistenten Folgezustand). Die SoC-Gamedata wird **nicht modifiziert** — der Fix liegt im Overlay (Original-Archiv unberührt, Drop-in-Prinzip bleibt gewahrt); entspricht dem Ansatz des Zone Reclamation Project (ZRP).

---

## 4. Revertierte Experimente (Begründung Drop-in-Prinzip)

| Experiment | Grund des Rückbaus | Ersetzt durch |
|---|---|---|
| `<hint_wnd>`-Node in Style-`pda.xml` eingefügt | Original hat keine; Style soll Original getreu bleiben | Engine-Fallback (2.8) |
| `color_defs.xml` (CS-Format) im loose Overlay + Style deployed | Schattiert Original-GSC-Daten wider das Drop-in-Prinzip; Original-Format decoded als korrekt (Extraktion bewies: identisches XML-Schema) | Engine-Toleranz (2.9) + RGB-Sync (2.10) |
| `pda_events.xml` ins Style-Verzeichnis kopiert | Redundant — multi-path-Suche findet die Datei im Archiv | Entfernt |

---

## 5. Werkzeuge & Workflows

- **`shoc_db_unpack.py`** — Python-Port von `trivial_encryptor` (LCG `m_seed = 0x08088405·seed+1`, alphabet-swap 1024× seed 6011979, Stream-XOR seed 24031979; RU-Variante 2048× seed 20091958) + LZHUF-Decoder (LZSS N=4096,F=60,THRESHOLD=2 + adaptives Huffman, exakte `d_code`/`d_len`/`p_len`/`p_code`-Tabellen). **Status:** Patch-Archive (dbc/dbd, 1.0006) funktionieren; Release-Archive (db0–db9) divergieren nach Byte 462 im Index-LZHUF-Stream — vermutlich anderes Packer-Vintage; Arbeitsliste hält `dump_config_file` parat.
- **`dump_config_file`-Trick:** Engine-Konsolenbefehl in `user.ltx` (wird beim Start ausgeführt): extrahiert beliebige Datei aus den (verschlüsselten) Archiven → so wurden `pda_events.xml`, `pda.xml`, `color_defs.xml`, `task_manager.script` (dbc) lesbar. Achtung: unregistrierte Konsolenzeilen werden beim Engine-Exit herausgeschrieben — Lines sind One-Shot.
- **gdb-Exception-Monitor (`exceptions.gdb`):** `catch throw` mit auto-Continue + Backtrace-Log nach `/tmp/shoc_exceptions.log`; fängt C++-Throws ohne Anhalten ( klassisches `catch throw` friert das Spiel ein).
- **Screenshot-Pixel-Analyse:** F12-Screenshots (`_appdata_/screenshots/`) + PIL-Testpunkte: Gold(231,168,12)-Detektion, Weiß-Text-Reihen-Sums im PDA-Bereich (ui-Skala W/1024, H/768 bei 2560×1440), Diff-Analyse gegen Original-Referenzscreenshots (Steam-Overlay: `~/.local/share/Steam/userdata/<id>/760/remote/4500/…`).

---

## 6. Verbleibende, offene Punkte

1. **Map-Tab (`eptMap`)** — `pUIMapWnd` = null, da `pda_map.xml` im SoC-Stile nicht existiert (CUIPdaWnd::Init Fallback bedingt das Fassungslos-GUI). Benötigt eine `pda_map.xml`-Adaption (analog der geleisteten Arbeit an `pda_tasks.xml`) oder virtuelles Fallback-Handling. Aktuell funktioniert die **Karte im Auftrags-Tab** (CoP-TaskWnd-Map, `pda_tasks.xml:map_wnd`), was die Kernanforderung „Karte + Regler" deckt.
2. **Original-Layout-Feinschliff** — Taskwindow-Feinjustierung gegen Original-Referenzscreenshots (Pixelfingerprint-Abgleich eltergen; Text-Pixelzeilen-Indizes vergleichen).
3. **Shutdown-Exception** — leere `std::exception` aus `main()` beim Beenden (Diff-Basis unklar; kein Core, keine sichtbaren Auswirkungen im Spiel); betrifft Debug nur kosmetisch.
4. **Release-Archive im Python-Unpacker** — LZHUF-Divergenz >462 Bytes (siehe 5).
5. **Scratch-Ordner im Repo** — `.shoc-extract/`, `.shoc-screens/`, `.shoc-ref/` sind Analyse-Kopien (in git ignoriert); vor Upstream-PR löschen.

---

## 7. Vorschlag Upstream-Commit-Aufteilung

Jeder Fix ist unabhängig revertierbar/reviewbar; vorgeschlagene Reihenfolge:

1. **Allocator:** `xrMemory.cpp` + `xrMemory_align.cpp` + `CMakeLists.txt` (kritisch, Linux-Blocker — sollte sofort in `dev`)
2. **ScrollView-Guard + InitScrollView-Aufrufe:** `UIScrollView.cpp` + `UISHoCTaskWnd.cpp/.h` + `UISHoCDiaryWnd.cpp`-Teil (PDA-Blocker)
3. **SoC-KI-/Monster-Toleranz:** `stalker_movement_params_inline.h` + `control_animation_base.cpp` (grossflächige SoC-Laufzeit-Blocker)
4. **Farb-Fallback + Scroll-Mappings:** `UIXmlInitBase.cpp` + `ui_base.cpp` (Voraussetzung:/thread-safe PDA/Inventar-Bau mit SoC-Ressourcen)
5. **Gametask-Toleranz + -TypDefault:** `GametaskManager.cpp` (SoC-Aufträge in CoP-UI)
6. **PDA-Struktur:** `UIPdaWnd.cpp` (Contacts-Platzhalter, CUITaskWnd im SHoC-Mode, Hint-Fallback) + Style-Ressourcen (`pda_tasks.xml`, ger-Stringtable — beide unsere Ressourcen)
7. **Kosmetik:** `Device_mode.cpp` (ImGui-Guard)

---

*Erstellt von Zed AI-Assistenz · Partner: itsgopnik (Plugin-Betrieb)*
