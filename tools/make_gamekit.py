#!/usr/bin/env python3
"""Assemble TempleFrog's game kit: what builds the game on the player's machine.

    python tools/make_gamekit.py --dest build/gamekit --recompiler <gba_recompile[.exe]>
                                 --zig <trimmed zig dir from tools/zig_toolchain.py>

The kit holds gba_recompile, the BIOS recompiler config, the module SDK
(gbarecomp/src/module) and the compiler. None of it is made from a ROM or
BIOS. KIT_ID changes whenever any of it does, which makes every player's
modules rebuild once (src/game_build.cpp).
"""
import argparse
import hashlib
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G = os.path.join(ROOT, "gbarecomp")
SDK_FILES = {
    "runtime_arm.h": os.path.join(G, "src", "module", "sdk", "runtime_arm.h"),
    "module_glue.cpp": os.path.join(G, "src", "module", "sdk", "module_glue.cpp"),
    "gbarecomp_module_abi.h": os.path.join(G, "src", "module", "gbarecomp_module_abi.h"),
    "native_program.h": os.path.join(G, "src", "armv4t", "native_program.h"),
    "runtime_wait_inline.h": os.path.join(G, "src", "armv4t", "runtime_wait_inline.h"),
    "runtime_wait_model.h": os.path.join(G, "src", "armv4t", "runtime_wait_model.h"),
    "armv4t_gba_runtime_arm_types.h": os.path.join(
        G, "external", "arm-recomp-core", "profiles", "armv4t_gba", "runtime_arm_types.h"),
}
# src/armv4t/runtime_arm_types.h reaches the profile header by a relative
# path; the kit has a flat include directory.
TYPES_SHIM = """// Game kit copy of gbarecomp/src/armv4t/runtime_arm_types.h.
#pragma once
#include "armv4t_gba_runtime_arm_types.h"
#define RUNTIME_TRACE_CODE_MISMATCH 9u
"""


def file_hash(h, path):
    with open(path, "rb") as f:
        h.update(os.path.basename(path).encode() + b"\0" + f.read())


def same_tree(a, b):
    va, vb = os.path.join(a, "VERSION"), os.path.join(b, "VERSION")
    return (os.path.exists(va) and os.path.exists(vb) and
            open(va).read() == open(vb).read())


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dest", required=True)
    ap.add_argument("--recompiler", required=True)
    ap.add_argument("--zig", required=True)
    args = ap.parse_args()

    types_src = os.path.join(G, "src", "armv4t", "runtime_arm_types.h")
    if "RUNTIME_TRACE_CODE_MISMATCH 9u" not in open(types_src).read():
        sys.exit("runtime_arm_types.h changed: update TYPES_SHIM in tools/make_gamekit.py")

    sdk = os.path.join(args.dest, "sdk")
    os.makedirs(sdk, exist_ok=True)
    h = hashlib.sha256()
    for name, src in sorted(SDK_FILES.items()):
        shutil.copy2(src, os.path.join(sdk, name))
        file_hash(h, src)
    with open(os.path.join(sdk, "runtime_arm_types.h"), "w", newline="\n") as f:
        f.write(TYPES_SHIM)
    h.update(TYPES_SHIM.encode())

    exe = os.path.basename(args.recompiler)
    shutil.copy2(args.recompiler, os.path.join(args.dest, exe))
    file_hash(h, args.recompiler)
    bios_cfg = os.path.join(G, "bios", "gba_bios.toml")
    shutil.copy2(bios_cfg, os.path.join(args.dest, "gba_bios.toml"))
    file_hash(h, bios_cfg)

    zig_dest = os.path.join(args.dest, "zig")
    if not same_tree(args.zig, zig_dest):
        if os.path.exists(zig_dest):
            shutil.rmtree(zig_dest)
        shutil.copytree(args.zig, zig_dest)
    file_hash(h, os.path.join(args.zig, "VERSION"))

    kit_id = h.hexdigest()[:16]
    with open(os.path.join(args.dest, "KIT_ID"), "w", newline="\n") as f:
        f.write(kit_id + "\n")
    print("game kit %s in %s" % (kit_id, args.dest))


if __name__ == "__main__":
    main()
