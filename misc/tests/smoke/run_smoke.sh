#!/usr/bin/env bash
# OpenXRay smoke test (Linux).
#
# Phases, each in a separate engine process:
#   menu  boot to the main menu, quit from the menu
#   play  new game, kill the nearest enemy, save, load the save in-process, quit
#   load  load the save from the command line (cold start), check the enemy is dead, quit
#
# The engine is driven by smoke_driver.lua through -test_script (Debug/Mixed builds only).
# Game data is not modified. The engine log, user.ltx and the smoke_test save are
# backed up/copied to the output directory and the originals restored afterwards.
#
# Usage: run_smoke.sh --game <dir with fsgame.ltx> --bin <dir with xr_3da>
#                     [--out <dir>] [--timeout <seconds per phase>] [--phases "menu play load"]
#                     [--gdb]  (run the engine under gdb; backtrace of a crash goes to <phase>.stdout.log)
#                     [--gdb-ex <command>]  (extra gdb command after a crash, repeatable; implies --gdb)
#                     [--headless]  (run inside an invisible kwin_wayland --virtual session with Xwayland
#                                    and a private D-Bus: no window, no focus stealing, no stray input)
#                     [-- <extra engine arguments, e.g. -soc or -r4>]

set -u

GAME=""
BIN=""
OUT=""
TIMEOUT=300
PHASES="menu play load"
GDB=()
GDB_EX=()
HEADLESS=0
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    case "$1" in
        --game) GAME=$2; shift ;;
        --bin) BIN=$2; shift ;;
        --out) OUT=$2; shift ;;
        --timeout) TIMEOUT=$2; shift ;;
        --phases) PHASES=$2; shift ;;
        --gdb) GDB=(gdb) ;;
        --gdb-ex) GDB=(gdb); GDB_EX+=("$2"); shift ;;
        --headless) HEADLESS=1 ;;
        -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ "${1:-}" = "--" ] && shift
EXTRA=("$@")
GDB_SCRIPT=()
if [ ${#GDB_EX[@]} -gt 0 ]; then
    # Extra commands run in hook-stop, i.e. only when the engine stops on a crash;
    # after a normal exit they would fail and turn gdb's exit code into 1.
    mkdir -p "${OUT:-/tmp}"
    gdb_cmds=$(mktemp /tmp/run_smoke_gdb.XXXXXX)
    { echo "define hook-stop"; printf '%s\n' "${GDB_EX[@]}"; echo "end"; } > "$gdb_cmds"
    GDB_SCRIPT=(-x "$gdb_cmds")
fi
if [ ${#GDB[@]} -gt 0 ]; then
    # 'thread apply all' doesn't fail after a normal exit, so the child's exit code survives
    GDB=(gdb -q -batch -return-child-result "${GDB_SCRIPT[@]}" -ex run -ex "thread apply all bt full" --args)
fi

[ -f "$GAME/fsgame.ltx" ] || { echo "No fsgame.ltx in --game '$GAME'" >&2; exit 2; }
[ -x "$BIN/xr_3da" ] || { echo "No xr_3da in --bin '$BIN'" >&2; exit 2; }

GAME=$(cd "$GAME" && pwd)
BIN=$(cd "$BIN" && pwd)
DRIVER=$(cd "$(dirname "$0")" && pwd)/smoke_driver.lua
OUT=${OUT:-/tmp/openxray-smoke-$(date +%Y%m%d-%H%M%S)}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

APPDATA="$GAME/_appdata_"
LOG="$APPDATA/logs/openxray_${USER}.log"
SAVES="$APPDATA/savedgames"

backup() { [ -f "$1" ] && cp -p "$1" "$OUT/backup_$(basename "$1")"; }
restore() { [ -f "$OUT/backup_$(basename "$1")" ] && cp -p "$OUT/backup_$(basename "$1")" "$1"; }

backup "$LOG"
backup "$APPDATA/user.ltx"
rm -f "$SAVES"/smoke_test.*
if [ "$HEADLESS" -eq 1 ] && [ -f "$APPDATA/user.ltx" ]; then
    # The window in the virtual session never gets input focus; without this the
    # engine pauses the level at start ("application start") and idles while
    # inactive. user.ltx is restored at the end.
    printf '\nrs_always_active on\n' >> "$APPDATA/user.ltx"
fi
: > "$OUT/results.txt"
rm -f "$OUT/state.txt"

run_phase() {
    local phase=$1; shift
    echo "=== phase $phase"
    local start_ts=$SECONDS
    # -silent_error_mode: abort on fatal errors instead of waiting for a dialog click
    local cmd=(timeout -k 15 "$TIMEOUT" "${GDB[@]}" "$BIN/xr_3da" -nointro -silent_error_mode
        -fsltx "$GAME/fsgame.ltx" -test_script "$DRIVER" "${EXTRA[@]}" "$@")
    if [ "$HEADLESS" -eq 1 ]; then
        # kwin_wayland takes a single session program: write one that runs the engine
        local session="$OUT/$phase.session.sh"
        {
            echo "#!/usr/bin/env bash"
            printf 'cd %q || exit 1\n' "$GAME"
            printf 'export LD_LIBRARY_PATH=%q XRAY_SMOKE_DIR=%q XRAY_SMOKE_PHASE=%q\n' \
                "$BIN${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$OUT" "$phase"
            # sanitizer builds: keep their options inside the nested session
            for var in ASAN_OPTIONS UBSAN_OPTIONS LSAN_OPTIONS; do
                [ -n "${!var:-}" ] && printf 'export %s=%q\n' "$var" "${!var}"
            done
            printf '%q ' "${cmd[@]}"
            echo
        } > "$session"
        chmod +x "$session"
        env -u WAYLAND_DISPLAY -u DISPLAY timeout -k 15 $((TIMEOUT + 60)) \
            dbus-run-session -- kwin_wayland --virtual --xwayland --no-lockscreen --no-global-shortcuts \
            --socket "xray-smoke-$$-$phase" --width 1280 --height 720 --exit-with-session "$session" \
            > "$OUT/$phase.stdout.log" 2>&1
    else
        (
            cd "$GAME" || exit 1
            export LD_LIBRARY_PATH="$BIN${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            export XRAY_SMOKE_DIR="$OUT" XRAY_SMOKE_PHASE="$phase"
            exec "${cmd[@]}"
        ) > "$OUT/$phase.stdout.log" 2>&1
    fi
    local rc=$?
    # timeout only signals its direct child (gdb, kwin): don't leave engines behind
    pkill -KILL -f -- "-test_script $DRIVER" 2>/dev/null
    pkill -KILL -f -- "--socket xray-smoke-$$-$phase" 2>/dev/null
    echo "$phase rc=$rc duration=$((SECONDS - start_ts))s" >> "$OUT/exit_codes.txt"
    [ -f "$LOG" ] && cp "$LOG" "$OUT/$phase.engine.log"
    if [ $rc -eq 124 ] || [ $rc -eq 137 ]; then
        echo "FAIL $phase timed out after ${TIMEOUT}s" >> "$OUT/results.txt"
    elif [ $rc -ne 0 ]; then
        echo "FAIL $phase engine exit code $rc" >> "$OUT/results.txt"
    elif ! grep -q "KERNEL:QUIT" "$OUT/$phase.engine.log" 2>/dev/null; then
        echo "FAIL $phase no clean shutdown in engine log" >> "$OUT/results.txt"
    else
        echo "PASS ${phase}_quit clean shutdown" >> "$OUT/results.txt"
    fi
}

: > "$OUT/exit_codes.txt"
for phase in $PHASES; do
    case "$phase" in
        menu) run_phase menu ;;
        # -nogameintro: otherwise the opening cinematic runs (~200 s, game time paused)
        # unless some input happens to skip it, which made visible runs non-deterministic
        play) run_phase play -nogameintro -start "server(all/single/alife/new)" "client(localhost)" ;;
        load)
            if [ -f "$SAVES/smoke_test.sav" ]; then
                run_phase load -start "server(smoke_test/single/alife/load)" "client(localhost)"
            else
                echo "FAIL load skipped, no smoke_test.sav from the play phase" >> "$OUT/results.txt"
            fi
            ;;
        *) echo "Unknown phase $phase" >&2 ;;
    esac
done

cp "$SAVES"/smoke_test.* "$OUT"/ 2>/dev/null
rm -f "$SAVES"/smoke_test.*
restore "$LOG"
restore "$APPDATA/user.ltx"

# Summary: driver results plus fatal/error lines from the engine logs
{
    echo "# Smoke test $(date -Iseconds)"
    echo
    echo "- Engine: $BIN"
    echo "- Game: $GAME"
    echo "- Extra args: ${EXTRA[*]:-(none)}"
    echo
    echo "## Results"
    echo
    sed 's/^/    /' "$OUT/results.txt"
    echo
    echo "## Exit codes"
    echo
    sed 's/^/    /' "$OUT/exit_codes.txt"
    for phase in $PHASES; do
        f="$OUT/$phase.engine.log"
        [ -f "$f" ] || continue
        echo
        echo "## $(basename "$f")"
        echo
        echo "- game mode: $(tr -d '\r' < "$f" | grep -m1 '^\* Game mode: [a-z]*$' | sed 's/^\* Game mode: //')"
        echo "- lines starting with '!': $(grep -c '^!' "$f")"
        echo "- FATAL/stack trace: $(grep -ciE 'fatal error|stack trace' "$f")"
        echo "- artefacts spawned by anomalies: $(grep -c '\] spawned artefact \[' "$f")"
        echo
        echo "Most frequent '!' lines:"
        echo
        tr -d '\r' < "$f" | grep '^!' | sed -E 's/[0-9]+/N/g' | sort | uniq -c | sort -rn | head -15 | sed 's/^/    /'
    done
} > "$OUT/report.md"

echo
cat "$OUT/results.txt"
echo "Report: $OUT/report.md"
! grep -q '^FAIL' "$OUT/results.txt"
