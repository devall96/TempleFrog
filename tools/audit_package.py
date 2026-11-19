#!/usr/bin/env python3
"""Fail a release package that carries anything made from the game.

    python tools/audit_package.py <folder> [--rom roms/frog_us.gba] [--bios bios.bin]

A TempleFrog package must hold no ROM or BIOS, no save or setting of the
player's, no personal path, and no code recompiled from the ROM or BIOS
(gbarecomp/docs/GAME_MODULES.md): the game is built on the player's machine. This looks
for:
  - files by type (.gba, .bin, .sav, ...) and by name (box art, settings);
  - the names the recompiler gives game functions (gf_afunc_08..., ram_aot_N,
    the BIOS's func_000...), which a build with the game linked in carries;
  - when --rom / --bios are given (a developer machine), 256 slices of the
    ROM's code and of the BIOS, which code copied from them would carry;
  - personal strings (--needle, repeatable).
Exit status 1 and one line per problem when anything is found.
"""
import argparse
import os
import re
import sys

BANNED_EXT = re.compile(r"\.(gba|agb|bin|sav|srm|state\d*|cfg|ini|replay|log)$", re.I)
BANNED_NAMES = {"boxart.tga", "backdrop.tga", "state.toml", "templefrog.ini"}
GAME_CODE = [
    re.compile(rb"gf_[at]func_0[0-9A-F]{7}"),       # cart functions
    re.compile(rb"gf_autojt_0[0-9A-F]{7}"),         # cart jump-table entries
    re.compile(rb"\bram_aot_[0-9]+\b"),             # copied-RAM routines
    re.compile(rb"\b[at]func_0000[0-9A-F]{4}\b"),   # BIOS functions
    re.compile(rb"swi_branch_table_[0-9]{2}"),      # BIOS SWI table
]


def slices(path, start, end, count=256, width=32):
    data = open(path, "rb").read()[start:end]
    if len(data) < width * 2:
        return []
    step = max(width, (len(data) - width) // count)
    out = []
    for off in range(0, len(data) - width, step):
        s = data[off:off + width]
        if len(set(s)) > 8:  # skip padding and fills that occur anywhere
            out.append(s)
    return out[:count]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("folder")
    ap.add_argument("--rom")
    ap.add_argument("--bios")
    ap.add_argument("--needle", action="append", default=[])
    args = ap.parse_args()

    needles = []
    if args.rom:
        needles += [("ROM", s) for s in slices(args.rom, 0x200, 0x36000)]
    if args.bios:
        needles += [("BIOS", s) for s in slices(args.bios, 0x0, 0x4000)]
    personal = [n.encode() for n in args.needle if n] + \
               [n.encode("utf-16-le") for n in args.needle if n]

    bad = []
    files = 0
    for dirpath, _, names in os.walk(args.folder):
        for name in names:
            path = os.path.join(dirpath, name)
            files += 1
            if BANNED_EXT.search(name):
                bad.append("banned file type: " + path)
            if name in BANNED_NAMES:
                bad.append("player or ROM-derived file: " + path)
            data = open(path, "rb").read()
            # Recompiler configs (gba_bios.toml) name functions; they hold no code.
            for pat in ([] if name.endswith(".toml") else GAME_CODE):
                m = pat.search(data)
                if m:
                    bad.append("game code (%s) in %s" % (m.group().decode(), path))
                    break
            for kind, s in needles:
                if s in data:
                    bad.append("bytes of the %s in %s" % (kind, path))
                    break
            low = data.lower()
            for n in personal:
                if n.lower() in low:
                    bad.append("personal string in %s" % path)
                    break
    for line in bad:
        print("AUDIT FAIL:", line)
    if not bad:
        checks = "names" + (", ROM" if args.rom else "") + (", BIOS" if args.bios else "")
        print("audit: clean (%d files; game code by %s)" % (files, checks))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
