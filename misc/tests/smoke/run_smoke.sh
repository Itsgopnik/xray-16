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
#                     [-- <extra engine arguments, e.g. -soc or -r4>]

set -u

GAME=""
BIN=""
OUT=""
TIMEOUT=300
PHASES="menu play load"
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    case "$1" in
        --game) GAME=$2; shift ;;
        --bin) BIN=$2; shift ;;
        --out) OUT=$2; shift ;;
        --timeout) TIMEOUT=$2; shift ;;
        --phases) PHASES=$2; shift ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ "${1:-}" = "--" ] && shift
EXTRA=("$@")

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
: > "$OUT/results.txt"
rm -f "$OUT/state.txt"

run_phase() {
    local phase=$1; shift
    echo "=== phase $phase"
    local start_ts=$SECONDS
    (
        cd "$GAME" || exit 1
        export LD_LIBRARY_PATH="$BIN${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        export XRAY_SMOKE_DIR="$OUT" XRAY_SMOKE_PHASE="$phase"
        exec timeout -k 15 "$TIMEOUT" "$BIN/xr_3da" -nointro -fsltx "$GAME/fsgame.ltx" \
            -test_script "$DRIVER" "${EXTRA[@]}" "$@"
    ) > "$OUT/$phase.stdout.log" 2>&1
    local rc=$?
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
        play) run_phase play -start "server(all/single/alife/new)" "client(localhost)" ;;
        load) run_phase load -start "server(smoke_test/single/alife/load)" "client(localhost)" ;;
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
