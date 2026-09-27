# Tools, Build-System & Externe Abhängigkeiten

> Deutsche Entwickler-Dokumentation des OpenXRay/xray-16-Projekts (Werkzeuge und Build).
> Pfade relativ zum Projekt-Stammverzeichnis.

## Einleitung

Zum Projekt gehören neben der Engine eigene **Werkzeuge** (Archiv-Packer, Level-Compiler, MP-Tools, Editoren), ein modernes **CMake-Build-System** und eine Reihe **externer Abhängigkeiten** (Submodule unter `Externals/`).

---

## 1. Build-System (CMake)

### Root-Projekt (`CMakeLists.txt`)

```cmake
cmake_minimum_required(VERSION 3.23)
project(OpenXRay
    VERSION 1.6.02.${XRAY_BUILD_ID}   # Build-ID via calculate_xray_build_id
    LANGUAGES CXX C)
option(BUILD_SHARED_LIBS "Build shared libraries" ...)
option(XRAY_USE_LUAJIT   "Use LuaJIT"      ON)
option(XRAY_USE_ASAN     "Use AddressSanitizer" OFF)
option(XRAY_ENABLE_TRACY "Enable tracy profiler" OFF)
add_subdirectory(Externals)
add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(misc)
```

### CMake-Module (`cmake/`)

| Datei | Aufgabe |
|-------|---------|
| `XRay.Build.cmake` | zentrale Build-Einstellungen (LTO, Unity-Build, Warnungen) |
| `XRay.Configurations.cmake` | Build-Konfigurationen: `Release`, `Debug`, `Mixed`, `ReleaseMasterGold` |
| `XRay.Compiler.GNULike.cmake` / `XRay.Compiler.MSVC.cmake` | Compiler-Spezifica (GCC/Clang vs. MSVC) |
| `XRay.Packaging.cmake` | Install-/Paketregeln |
| `PreProjectInit.cmake` | Vorprojekt-Initialisierung (u. a. `calculate_xray_build_id`) |
| `utils.cmake` | Hilfsfunktionen (u. a. `target_sources_grouped`) |
| `FindLZO.cmake`, `FindOgg.cmake`, `FindVorbis.cmake`, `FindTheora.cmake` | Suche nach System-Bibliotheken (LZO, Ogg, Vorbis, Theora) |
| `cmake_uninstall.cmake.in` | Uninstall-Target |

### Konfigurationen & Besonderheiten

- **`ReleaseMasterGold`**: definiert `MASTER_GOLD` und `XRAY_EXCEPTIONS=0` (alle anderen Konfigurationen: `XRAY_EXCEPTIONS=1`), definiert in `src/CMakeLists.txt`.
- **Statisch vs. Shared**: ohne `BUILD_SHARED_LIBS` wird `XRAY_STATIC_BUILD` gesetzt – die Export-Makros (`XRAPI_API`, `XRRENDER_*_API`) schalten dann auf interne Verlinkung um.
- **Ausgabe**: `bin/{CMAKE_SYSTEM_PROCESSOR}/{CONFIG}/`; LTO aktiv, sofern unterstützt; Unity-Build möglich.
- **MSVC-Reste**: `src/engine.sln`, `*.vcxproj`, `*.props` stammen aus der Visual-Studio-Ära und sind für den CMake-Build nicht relevant (Solution-Wartung).

### Build-Befehle (Linux)

```bash
# Submodule initialisieren (einmalig)
git submodule update --init --recursive

# Release-Build konfigurieren
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DXRAY_USE_LUAJIT=ON

# Kompilieren
cmake --build build -j$(nproc)
# Ergebnis: build/bin/<Arch>/Release/xr_3da

# Debug-Build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON
cmake --build build-debug -j$(nproc)

# Mit AddressSanitizer
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DXRAY_USE_ASAN=ON
cmake --build build-asan -j$(nproc)
```

### Target-Übersicht (alle Module)

| Target | Plattform | Typ |
|--------|-----------|-----|
| `xr_3da` | alle | Hauptprogramm |
| `xrCore`, `xrEngine`, `xrCDB`, `xrPhysics`, `xrParticles`, `xrSound` | alle | Bibliothek |
| `xrScriptEngine`, `xrUICore`, `xrAICore` | alle | Bibliothek |
| `xrGame`, `xrGameSpy`, `xrNetServer`, `xrMaterialSystem` | alle | Bibliothek |
| `xrAPI` | alle | Bibliothek (`GEnv`) |
| `xrRender_GL` | alle | Renderer (Standard) |
| `xrRenderPC_R4` | nur Windows | Renderer (DX11, WIP im CMake-Build) |
| `xrImGui` | alle | Dev-UI-Bindung (für Renderer/UI) |
| Utils (siehe unten) | alle | Einzelprogramme |

---

## 2. Externe Abhängigkeiten (`Externals/`)

Git-Submodule; eingebunden über `Externals/CMakeLists.txt`.

| Submodul | Zweck | Genutzt von |
|----------|-------|-------------|
| `LuaJIT` (+ `xrLuaFix`) | Just-in-time-Compiler für Lua 5.1 | `xrScriptEngine` (Option `XRAY_USE_LUAJIT`, sonst reines Lua 5.1) |
| `luabind` (de-boostified) | C++-↔-Lua-Bindings | `xrGame`, `xrServerEntities`, Renderer-Scripting |
| `ode` | Open Dynamics Engine (Physik) | `xrPhysics` (Include-Pfad `Externals/ode/include` in xrGame/xrGameSpy/xrPhysics referenziert) |
| `OPCODE` | Triangle-Mesh-Kollision | Physik/Rendering (Level-Kollision) |
| `GameSpy` | Multiplayer-SDK (QR2, GCD, GP, ATLAS) | `xrGameSpy`, `xrGame` |
| `gli` | OpenGL-Image-Bibliothek (DDS etc.) | Renderer (TEXTUREN-Laden, Include-Pfad in `xrRenderPC_GL`) |
| `imgui` (+ `imgui-proj`) | Developer-UI | `xrImGui`-Target, Renderer (`dxImGuiRender`) |
| `tracy` | Echtzeit-Profiler | alle Module mit `ZoneScoped` (Option `XRAY_ENABLE_TRACY`) |
| `sse2neon` / `sse2rvv` | SSE-SIMD → ARM-NEON / RISC-V-Translation | portabler SSE-Code ( skeletal blending, Mathematik) |
| `DiscordGameSDK` | Discord Rich Presence | `CApplication` (`x_ray.cpp`) |
| `renderdoc` | Grafik-Debug-Anbindung | Renderer (API-Includes) |
| `AGS_SDK` | Zusatz-SDK (Submodul; Details älterer GPU-Integration) | v. a. Windows/Vorselektion – nicht im Kernpfad |
| `submodule_check.hpp` | Header zur Submodul-Prüfung | Build-System |

**Keine Submodule, sondern System-Bindungen:** SDL2 (Fenster/Input, via `xrEngine`), OpenAL (Audio, `xrSound`), Ogg/Vorbis (Streaming-Sounds via `Find*.cmake`), Theora (Videos in `xrEngine`), LZO (Kompression, `FindLZO.cmake`), glad (GL-Loader: `sdk/include/glad/`), DirectXMesh-Header (`sdk/include/DirectXMesh/`).

---

## 3. Werkzeuge (`src/utils/`, `src/editors/`)

### Einzelprogramme (Target je Tool)

| Ordner | Aufgabe |
|--------|---------|
| `src/utils/xrCompress` | Packen der Spielarchive (`.db`/`.xdb`-Pakete) |
| `src/utils/xrLC_Light` | Level-Compiler: Lightmap-/GEO-Vorberechnung (nutzt gemeinsam genutzte Renderer-Quellen, z. B. `R_light.h`; wird vom Build der Renderer mit `../../utils/xrLC_Light/R_light.h` referenziert) |
| `src/utils/xrMiscMath` | Mathe-Bibliothek/CLI-Werkzeuge (wird z. B. vom R4-Renderer gelinkt) |
| `src/utils/mp_balancer` | Multiplayer-Werkzeug (Balance-Analysen) |
| `src/utils/mp_configs_verifyer` | Multiplayer-Werkzeug (Config-Prüfung) |
| `src/utils/mp_gpprof_server` | Multiplayer-Werkzeug (Profiler-Server) |
| `src/utils/xrLoadSurface.cpp` | Einzeldatei-Werkzeug (Surface-Helfer) |

### Editoren (`src/editors/`)

| Ordner | Aufgabe |
|--------|---------|
| `xrWeatherEditor` | Wetter-Daten-Editor |
| `xrWeatherEngine` | Engine-Anbindung des Wetter-Editors (nutzt die Editor-Schnittstellen `src/Include/editor/engine.hpp`, `ide.hpp`, `interfaces.hpp`, `property_holder_base.hpp`) |
| `xrSdkControls` | SDK-Steuer-Elemente (Editor-Basis) |

Die Editoren sind nicht Teil des CMake-Hauptbuilds; sie greifen auf die (im `src/Include/editor` deklarierten) Editor-Schnittstellen von `xrEngine` zurück (`editor_base.cpp/engine`).

> `src/dummy` (nur `00dummy_tester.cpp`) ist ein Platzhalter-Target der Visual-Studio-Solution.

---

## 4. CI, Packaging & Diverses

| Pfad | Aufgabe |
|------|---------|
| `.github/workflows/cibuild.yml` | Haupt-CI: baut alle Plattformen (Windows/Linux/macOS/BSD, x86/ARM) |
| `.github/workflows/stylecheck.yml` | Code-Style-Prüfung |
| `.github/workflows/labeler.yml`, `mirror.yml` | Issue-Labeling, Repository-Mirroring |
| `appveyor.yml` | Legacy-CI (AppVeyor) |
| `ArchLinux/`, `debian/` | Distributions-Paketierung (PKGBUILD, control files) |
| `res/`, `misc/` | Ressourcen/Medien des Builds (Icons etc.) |
| `sdk/include/` | mitgelieferte SDK-Header (`glad`, `KHR`, `DirectXMesh`) |
| `doc/doxygen/Doxyfile` | Doxygen-Konfiguration (API-Dokumentation generierbar) |
| `doc/procedure/` | Projekt-Richtlinien: `cpp_code.txt`, `cpp_comment.txt`, `commit_coding.txt`, `common.txt` |
| `doc/design/` | Historische Design-Aufzeichnungen (`task_list.txt`, `task_history.txt`) |
| `doc/soc/` | Materialien zum SoC-Support |

---

## Hinweise für Entwickler & Modder

1. **Build-Hygiene:** Nur CMake nutzen; die VS-Solution-Dateien spiegeln den ursprünglichen GSC-Build und nicht den Linux/CMake-Pfad.
2. **Neue Dateien** immer in die betreffende `CMakeLists.txt` aufnehmen (xrCore/xrEngine nutzen `target_sources_grouped`, die Renderer-Targets explizite Quelllisten).
3. **Doxygen:** mit `doc/doxygen/Doxyfile` lässt sich eine klassenbasierte API-Doku erzeugen – diese `docs/`-Sammlung ist davon unabhängig.
4. **Stil:** `doc/procedure/cpp_*.txt` beschreibt Kommentar- und Format-Konventionen des Projekts (historisch).

---
*Basis: `CMakeLists.txt` (Root, `src/`, `Externals/`-Verzeichnis), `cmake/`-Liste, `src/CMakeLists.txt` (Konfigurationen), `xrRenderPC_GL/CMakeLists.txt` (SDK/gli/xrLC-Referenzen), `.github/workflows/`, Verzeichnislisten (`Externals`, `src/utils`, `src/editors`, `doc`, `sdk/include`).*
