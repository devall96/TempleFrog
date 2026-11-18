#!/usr/bin/env bash
# Linux/macOS counterpart of tools/regen.ps1: verify the ROM and generate the
# native code into generated/ (and the BIOS code, if a BIOS is supplied).
#
#   tools/regen.sh [--rom roms/frog_us.gba] [--gbarecomp gbarecomp] [--max-functions 65536]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rom="$root/roms/frog_us.gba"
engine="$root/gbarecomp"
max_functions=65536
while [[ $# -gt 0 ]]; do
    case "$1" in
        --rom) rom="$2"; shift 2 ;;
        --gbarecomp) engine="$2"; shift 2 ;;
        --max-functions) max_functions="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

engine="$(cd "$engine" && pwd)"
tool="$engine/build/gba_recompile"
if [[ ! -x "$tool" ]]; then
    echo "Missing $tool. Build the engine's gba_recompile target first." >&2
    exit 1
fi
[[ -f "$rom" ]] || { echo "ROM not found: $rom" >&2; exit 1; }

expected=7b4c27009198df18555e63fb5dcad223eaf09815
actual="$(sha1sum "$rom" | cut -d' ' -f1)"
if [[ "$actual" != "$expected" ]]; then
    echo "Frogger's Adventures ROM SHA-1 mismatch: got $actual expected $expected" >&2
    exit 1
fi

# Generated BIOS (LLE). Needs the user's own BIOS at gbarecomp/bios/gba_bios.bin.
bios="$engine/bios/gba_bios.bin"
bios_out="$engine/src/runtime/generated_bios"
if [[ -f "$bios" && ! -f "$bios_out/bios_recompiled.cpp" ]]; then
    "$tool" --bios "$bios" --config "$engine/bios/gba_bios.toml" --out "$bios_out"
fi

"$tool" --rom "$rom" --config "$root/game.toml" \
    --out "$root/generated" --max-functions "$max_functions"
