# SoC-PDA: benötigte Funktionen und aktueller Stand

Arbeitsdokument zum Shadow-of-Chernobyl-PDA in OpenXRay (Tracker: #392, Punkt „UI → PDA“).
Stand: Branch `soc_dev2`, 2026-09-24 (nach dem Umbau der vier SoC-Fenster).

## Referenz: Original-Code

Die ursprünglichen SoC-PDA-Fenster lagen früher im CoP-Quellcode, wurden aber nicht mehr verwendet.
In Commit `28cd2555a` („Clean includes, delete unused sources“, 2015) wurden sie gelöscht.
Sie sind die beste Referenz dafür, wie sich das PDA verhalten soll:

```
git show 28cd2555a~1:src/xrGame/ui/UIEventsWnd.cpp        # Aufgaben
git show 28cd2555a~1:src/xrGame/ui/UITaskItem.cpp         # Aufgaben-Einträge
git show 28cd2555a~1:src/xrGame/ui/UITaskDescrWnd.cpp     # Aufgaben-Beschreibung
git show 28cd2555a~1:src/xrGame/ui/UIDiaryWnd2.cpp        # Tagebuch
git show 28cd2555a~1:src/xrGame/ui/UINewsWnd.cpp          # Nachrichten
git show 28cd2555a~1:src/xrGame/ui/UIPdaContactsWnd.cpp   # Kontakte
git show 28cd2555a~1:src/xrGame/ui/UIStalkersRankingWnd.cpp  # Rangliste
git show 28cd2555a~1:src/xrGame/ui/UIEncyclopediaWnd.cpp  # Enzyklopädie
```

Die Original-XMLs von SoC (`pda_events.xml`, `events_new.xml`, `pda_contacts_new.xml`,
`stalkers_ranking.xml`, `job_item.xml` …) liegen in den `gamedata.db*`-Archiven. Mit dem
Konsolenbefehl `dump_config_file` lassen sie sich auf die Platte schreiben, z.B.
`dump_config_file ui\pda_events.xml /tmp/pda_events.xml`.

## Architektur

`CUIPdaWnd` (`src/xrGame/ui/UIPdaWnd.cpp`) ist die Hülle mit Hintergrund, Uhr, Tab-Leiste und Close-Button.
Upstream hat sie für SoC schon angepasst (`fdc7ead53`).
Im SoC-Modus (`ShadowOfChernobylMode`) werden statt der CoP-Fenster die SoC-Fenster erzeugt.
Die Tab-Buttons in SoC haben die IDs `"0"`–`"6"`, die `CUIPdaWnd::Init` auf die Namen unten umschreibt:

| Tab | ID | Fenster (SoC-Modus) | Original-Klasse |
|---|---|---|---|
| 0 | `eptTasks` | `CUISHoCTaskWnd` | `CUIEventsWnd` |
| 1 | `eptMap` | `CUIMapWnd` (gemeinsam mit CoP) | `CUIMapWnd` |
| 2 | `eptDiary` | `CUISHoCDiaryWnd` | `CUIDiaryWnd` |
| 3 | `eptContacts` | `CUISHoCContactsWnd` | `CUIPdaContactsWnd` |
| 4 | `eptStalkersRanking` | `CUISHoCRankingWnd` | `CUIStalkersRankingWnd` |
| 5 | `eptStatistics` | `CUIActorInfoWnd` (gemeinsam mit CoP) | `CUIActorInfoWnd` |
| 6 | `eptEncyclopedia` | `CUIEncyclopediaWnd` | `CUIEncyclopediaWnd` |

Ein Tab bleibt leer, wenn das zugehörige Fenster in `Init()` fehlschlägt. Dann wird es gelöscht
(`xr_delete`) und `SetActiveSubdialog` findet kein Fenster dafür.

## Die Tabs im Einzelnen

Legende: ✅ umgesetzt · 🟡 teilweise · ❌ fehlt

Alle vier SoC-Fenster lesen ihr Layout jetzt aus den Original-XMLs, keine Position ist mehr im Code
hart kodiert. Das Muster ist dasselbe wie in `CUIEncyclopediaWnd`: `UIHelper::Create*(xml, …, false)`,
fehlende Knoten werden übersprungen.

### 0 · Aufgaben (`CUISHoCTaskWnd`, `CUISHoCTaskItem`)

**Daten:** `Level().GameTaskManager().GetGameTasks()`. Objective 0 ist die Aufgabe selbst (`m_Title`,
Icon, Zeiten), ab 1 folgen die Teilziele (`m_Description`). Die Beschreibung kommt, wie im Original,
aus den `eTaskArticle`-Artikeln, deren Gruppe mit der Aufgaben-ID beginnt, plus `m_article_id`.

| Funktion (Original) | Stand |
|---|---|
| Liste: Aufgabe als Root-Eintrag, darunter alle Teilziele | ✅ |
| Filter-Tabs „Aktiv / Erledigt / Gescheitert“ | ✅ |
| Icon, Titel, Datum und Uhrzeit des Erhalts | ✅ |
| Restzeit bei Aufgaben mit Zeitlimit (`ui_st_time_remains`) | ✅ |
| Teilziele mit Status-Icon und Farbe aus `job_item.xml` | ✅ |
| Blinkendes Symbol am aktiven Teilziel, Doppelklick macht ein Teilziel aktiv | ✅ |
| Rechts Karte mit dem Ziel des gewählten Teilziels (`CUIMapWnd`), Umschalter zur Beschreibung | ✅ |
| Liste aktualisiert sich, wenn Aufgaben hinzukommen oder ihren Status ändern | ✅ (Prüfsumme über alle Zustände in `Update()`) |
| Hinweise (Tooltips) an den Buttons | ❌ |

### 1 · Karte (`CUIMapWnd`)

Das ist die CoP-Klasse, die mit `pda_map.xml` initialisiert wird. **Status: ungetestet.** Zu prüfen
sind die Aufgaben-Marker (nutzen die Ersatz-Icons aus `ui_base.cpp`), Zoom und Legende.

### 2 · Tagebuch (`CUISHoCDiaryWnd`)

**Daten:** Nachrichten über `Actor()->game_news_registry`, Journal über
`Actor()->encyclopedia_registry` mit `ARTICLE_DATA::eJournalArticle`.

| Funktion (Original) | Stand |
|---|---|
| Filter-Tabs Journal / Nachrichten | ✅ |
| Journal: `eJournalArticle`-Artikel nach Gruppen, gelesen/ungelesen, rechts der Text | ✅ (gemeinsamer Code mit der Enzyklopädie, `CUIArticleBrowser`) |
| Nachrichten neueste zuerst, `news_item`/`talk_item` aus `news.xml` über `CUINewsItemWnd` | ✅ |
| Neue Nachrichten erscheinen, ohne den Tab neu zu öffnen | ✅ |
| Info-Portion `ui_pda_news_hide` beim Verlassen der Nachrichten | ✅ |
| Symbole neu/gelesen (`updated_section_static` / `old_section_static`) | 🟡 ungelesene Einträge werden farbig markiert statt mit Symbol |

### 3 · Kontakte (`CUISHoCContactsWnd`, `CUISHoCContactItem`)

**Daten:** `Actor()->GetPDA()->ActivePDAContacts(list)`, also die Charaktere, deren PDA gerade in
Reichweite ist.

| Funktion (Original) | Stand |
|---|---|
| Liste der PDA-Kontakte in Reichweite | ✅ |
| Liste aktualisiert sich, wenn Kontakte kommen oder gehen | ✅ (Auswahl bleibt erhalten) |
| Eintrag als Charakter-Karte aus `pda_character.xml` (Portrait, Name, Fraktion, Ruf, Einstellung, Rang) | ✅ |
| Rechts Biografie des gewählten Kontakts | ✅ (im Original auskommentiert, jetzt umgesetzt) |

### 4 · Rangliste (`CUISHoCRankingWnd`, `CUISHoCRankingItem`)

**Daten:** Alle lebenden `CSE_ALifeHumanStalker` aus ALife plus der Spieler, sortiert nach Rang.
Die Original-Liste `g_all_statistic_humans` gibt es nicht mehr.

| Funktion (Original) | Stand |
|---|---|
| Top 20 mit Platz, Name und Rangwert (`item_human`) | ✅ |
| Eigener Eintrag des Spielers (`item_actor`), bei Platz > 20 mit „…“ (`item_ellipsis`) | ✅ |
| Klick zeigt `CUICharacterInfo` (`stalkers_ranking_character.xml`) | ✅ |
| Leere Liste (`no_items`) | ✅ |

### 5 · Statistik (`CUIActorInfoWnd`)

Das ist die CoP-Klasse mit dem Layout `actor_statistic.xml` aus `ui_style_shoc`. Für die SoC-Fraktionsliste gibt es einen Fallback auf den Knoten
`_relation_communities_list`. **Status: ungetestet.**

### 6 · Enzyklopädie (`CUIEncyclopediaWnd`)

**Daten:** `Actor()->encyclopedia_registry` mit `ARTICLE_DATA::eEncyclopediaArticle`. Die Listen- und
Artikellogik liegt jetzt in `CUIArticleBrowser` und wird vom Tagebuch mitbenutzt.

| Funktion (Original) | Stand |
|---|---|
| Artikel nach Gruppen, auf- und zuklappbar | ✅ |
| Artikeltext mit Bild (`encyclopedia_item.xml`) | ✅ |
| Gelesen/ungelesen | ✅ |
| Trennlinien (`left_auto_static` / `right_auto_static`) | ✅ (wurden vorher nicht gezeichnet, siehe unten) |

## Übergreifende Punkte

- **Wiederholte Knoten auf oberster Ebene.** SoC-XMLs nutzen z.B. mehrere `<left_auto_static>`-Knoten
  nebeneinander. Das Original hatte dafür `InitAutoStatic`, das es nicht mehr gibt.
  `InitAutoStaticGroup` erwartet dagegen `<auto_static>`-Kinder. Neu: `UIHelper::CreateStatics(xml, name, parent)`.
- **Auswählbare Listen.** `CUIScrollView::SetSelected` castet jeden Eintrag ungeprüft auf
  `CUISelectable`. In Listen mit `can_select="1"` müssen deshalb alle Einträge davon erben.
- **Klicks auf Buttons in Listeneinträgen.** Ein Fenster bekommt `OnMouseDown` vor seinen Kindern.
  `CUISHoCTaskItem` lässt Klicks auf seine Buttons deshalb ausdrücklich durch.
- **`CUIMapWnd` nicht vor dem Level anzeigen.** `Show(true)` während `Init()` stürzt in `Activated()` ab.
  Die Karte wird in `Init()` nur angehängt.
- **Scrollbar-Texturen.** `ui_base.cpp` registriert jetzt `_back/_b/_e` für die SoC-IDs `ui_scroll_*`
  aus `ui_common.xml` sowie die Zustände `_d/_t/_h` der PDA-Scroll-Schieber.
- **Überflüssige Style-Dateien (offen).** `pda_tasks.xml`, `pda_logs.xml` und `pda_ranking.xml` in
  `ui_style_shoc` sind im CoP-Format und werden im SoC-Modus nicht gelesen. `backup.xml` ist eine Kopie
  von `actor_statistic.xml`.

## Vergleich: Original-Bausteine ↔ heute

Die meisten Widgets, die die Original-Fenster verwendet haben, gibt es noch und werden wiederverwendet:

| Baustein im Original | Heute | Verwendet für |
|---|---|---|
| `CUICharacterInfo` | ✅ `src/xrGame/ui/UICharacterInfo.h` (`InitCharacterInfo`, `InitCharacter(id)`) | Kontakte, Rangliste |
| `CUINewsItemWnd` | ✅ `src/xrGame/ui/UINewsItemWnd.h` (`Init(xml, node)`, `Setup(GAME_NEWS_DATA&)`) | Tagebuch → Nachrichten (`news.xml`) |
| `CUIMapWnd` | ✅ | Aufgaben → Karten-Modus |
| `CUITabControl`, `CUIScrollView`, `CUIAnimatedStatic`, `CUIFrameWindow`, `CUIFrameLineWnd` | ✅ `xrUICore` | Filter, Listen, Rahmen |
| `CUIListWnd` | 🟡 existiert, aber veraltet, besser `CUIListBox` | Listen |
| `CUITreeViewItem` | ❌ | Journal-Baum. Ersatz: gruppierte `CUIListBox` in `CUIArticleBrowser` |
| `CUITaskItem`, `CUITaskDescrWnd` | ❌ | neu geschrieben als `CUISHoCTaskItem` bzw. im Aufgaben-Fenster |

XML-Knoten, die die Originale gelesen haben:

| Fenster | Datei | Knoten |
|---|---|---|
| Aufgaben | `pda_events.xml` | `main_wnd`, `main_wnd:left_frame` (+ `:left_frame_header`, `:left_frame_header:anim_static`, `:list`, `:filter_tab`), `main_wnd:right_frame` (+ `:map_wnd`, `:task_descr_view`) |
| Aufgaben-Einträge | `job_item.xml` | `task_root_item` (`:image`, `:caption`, `:caption_time`, `:rem_time`, `:switch_description_btn`), `task_sub_item` (`:state_image`, `:description`, `:show_descr_btn`, `:active_objecttive_image`) |
| Tagebuch | `events_new.xml` | `main_wnd:left_frame` (+ `:left_frame_header:filter_tab`, `:work_area:src_list`, `…:tree_item_font`, `…:tree_root_font`), `main_wnd:right_frame` (+ `:work_area:scroll_view`), `updated_section_static`, `old_section_static` |
| Nachrichten | `news.xml` | Root-Knoten mit Scroll-View, Einträge über `CUINewsItemWnd` |
| Kontakte | `pda_contacts_new.xml` | `main_wnd`, `left_frame_window`, `left_frame_line`, `right_frame_window`, `right_frame_line`, `a_static`, `list`, `detail_list`, `left_auto_static`, `right_auto_static` |
| Rangliste | `stalkers_ranking.xml` + `stalkers_ranking_character.xml` | `main_wnd`, `info_frame_window`, `info_frame_line`, `chicon_frame_window`, `chicon_frame_line`, `a_static`, `list`, `character_info`, `left_auto`, `right_auto`, Einträge `item_human`/`item_actor`/`item_ellipsis` (`text_1..3`, `auto`) |
| Enzyklopädie | `encyclopedia.xml` | `main_wnd`, `left/right_frame_window`, `left/right_frame_line`, `a_static`, `idx_list`, `info_list`, `article_header_static`, `left/right_auto_static` |

`CUIEncyclopediaWnd` liest bereits dieselben Knoten wie das Original, nur mit heutigen Widgets.
Kontakte und Rangliste nutzen dasselbe Knotenschema (`*_frame_window`, `*_frame_line`, `a_static`,
`list`) und lassen sich nach demselben Muster umbauen.

Nicht nachbauen: Die „eigenen Aufgaben“ im Original (`task_user_item`, `edit_user_item`, Filter
`eOwnTask`) waren schon im fertigen SoC auskommentiert.

## Offene Punkte

1. Im Spiel testen: alle Tabs, besonders Aufgaben (Karte/Beschreibung, Filter, Doppelklick) und Kontakte
2. Karte und Statistik prüfen
3. Tooltips an den Buttons im Aufgaben-Tab (`hint`-Attribute in `pda_events.xml` / `job_item.xml`)
4. Symbole neu/gelesen im Tagebuch
5. Überflüssige Dateien in `ui_style_shoc` klären

## Testen

Ein neues Spiel in der Testkopie starten und direkt bei Sidorovich das PDA öffnen. Ein alter
Spielstand kann an Skript-Fehlern im Aufgabensystem scheitern (siehe #392). Danach alle Tabs
durchklicken und im Log nach neuen `!`- und `FATAL`-Zeilen suchen.

Direkt ins neue Spiel, ohne Hauptmenü:

```
xr_3da -nointro -start 'server(all/single/alife/new)' 'client(localhost)'
```
