#!/usr/bin/env bash
# Deploy the SHoC UI style (ui_style_shoc) and localization overlay from the
# OpenXRay source tree into a S.T.A.L.K.E.R. - Shadow of Chernobyl install,
# so the engine can find them at runtime.
#
# Source tree uses `configs/ui/styles_/` (plural, trailing underscore);
# runtime expects `config/ui/styles/` (singular, no underscore). This script
# bridges that mismatch.
#
# Usage:
#   install_ui_style_shoc.sh [TARGET_GAME_DIR]
#
# TARGET_GAME_DIR defaults to:
#   ~/.local/share/Steam/steamapps/common/STALKER Shadow of Chernobyl
#
# Files installed:
#   <target>/gamedata/config/ui/styles/ui_style_shoc/*.xml
#   <target>/gamedata/config/text/eng/string_table_z_shoc_overlay.xml

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(cd -- "${SCRIPT_DIR}/../../res/gamedata" && pwd)"

DEFAULT_TARGET="${HOME}/.local/share/Steam/steamapps/common/STALKER Shadow of Chernobyl"
TARGET_GAME_DIR="${1:-${DEFAULT_TARGET}}"

if [[ ! -d "${TARGET_GAME_DIR}" ]]; then
    echo "error: target game dir not found: ${TARGET_GAME_DIR}" >&2
    echo "usage: $0 [TARGET_GAME_DIR]" >&2
    exit 1
fi

SRC_STYLE="${SOURCE_ROOT}/configs/ui/styles_/ui_style_shoc"
SRC_OVERLAY="${SOURCE_ROOT}/configs/text/eng/string_table_z_shoc_overlay.xml"

DEST_STYLE="${TARGET_GAME_DIR}/gamedata/config/ui/styles/ui_style_shoc"
DEST_OVERLAY_DIR="${TARGET_GAME_DIR}/gamedata/config/text/eng"

if [[ ! -d "${SRC_STYLE}" ]]; then
    echo "error: source style dir not found: ${SRC_STYLE}" >&2
    exit 1
fi
if [[ ! -f "${SRC_OVERLAY}" ]]; then
    echo "error: source overlay not found: ${SRC_OVERLAY}" >&2
    exit 1
fi

mkdir -p "${DEST_STYLE}"
mkdir -p "${DEST_OVERLAY_DIR}"

# Copy XMLs, skip backup.xml (working scratch file, not a deliverable).
copied=0
for src in "${SRC_STYLE}"/*.xml; do
    name="$(basename "${src}")"
    if [[ "${name}" == "backup.xml" ]]; then
        continue
    fi
    cp -v "${src}" "${DEST_STYLE}/${name}"
    copied=$((copied + 1))
done

cp -v "${SRC_OVERLAY}" "${DEST_OVERLAY_DIR}/"
copied=$((copied + 1))

echo
echo "installed ${copied} file(s) into ${TARGET_GAME_DIR}"
echo "activate in-game via console: ui_style ui_style_shoc; ui_restart"
