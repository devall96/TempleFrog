#!/usr/bin/env python3
"""The compiler TempleFrog ships to build the game on the player's machine.

A TempleFrog release holds no code made from the game. The player's ROM and
BIOS are recompiled and compiled into game modules on their own machine
(gbarecomp/docs/GAME_MODULES.md), with Zig's bundled clang. This script fetches the
pinned Zig release, checks its SHA-256, and trims it to what a Windows or
Linux x86_64 module build needs (about a quarter of its size).

    python tools/zig_toolchain.py --platform windows --dest out/zig-windows
    python tools/zig_toolchain.py --platform linux --dest out/zig-linux [--archive zig.tar.xz]

--archive uses an already downloaded release (checked against the same hash).
Zig is MIT-licensed; its LICENSE and the licences of the parts it carries
(LLVM/clang, mingw-w64, glibc headers, libc++ headers) stay in the tree.
"""
import argparse
import hashlib
import io
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

VERSION = "0.17.1"
RELEASES = {
    "windows": ("zig-x86_64-windows-%s.zip" % VERSION,
                "b03f477a1943673e02aca0f570e8030dffd536ce1f45cbc4d3326ec31159562e"),
    "linux": ("zig-x86_64-linux-%s.tar.xz" % VERSION,
              "3581863e3cbcfd0fce0efc682ce7e9e93da96668a977791d356a26d2a7e9b17a"),
}
URL = "https://ziglang.org/download/%s/%s"

# lib/ entries every build needs: the standard library (compiler_rt is built
# from it), clang's own headers and the libc++ headers (<cstdint> and friends;
# modules link no C++ runtime).
KEEP_COMMON = ["std", "compiler_rt", "compiler_rt.zig", "c", "c.zig", "include", "zig.h",
               "libcxx/include", "libcxx/LICENSE.TXT", "libcxxabi/include",
               "libcxxabi/LICENSE.TXT", "compiler"]
# lib/libc entries per target.
KEEP_LIBC = {
    "windows": ["mingw", "include/any-windows-any"],
    "linux": ["glibc", "include/generic-glibc", "include/any-linux-any",
              "include/x86-linux-any", "include/x86-linux-gnu"],
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch(platform, archive):
    name, digest = RELEASES[platform]
    if archive:
        with open(archive, "rb") as f:
            data = f.read()
    else:
        url = URL % (VERSION, name)
        print("downloading", url, file=sys.stderr)
        with urllib.request.urlopen(url) as r:
            data = r.read()
    if sha256(data) != digest:
        sys.exit("%s: SHA-256 mismatch (want %s)" % (name, digest))
    return name, data


def extract(name, data, into):
    if name.endswith(".zip"):
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            z.extractall(into)
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:xz") as t:
            try:
                t.extractall(into, filter="data")
            except TypeError:  # Python < 3.12 (AlmaLinux 8's 3.6); the archive's hash is pinned
                t.extractall(into)
    (root,) = os.listdir(into)
    return os.path.join(into, root)


def copy_entry(src_root, dst_root, rel):
    src = os.path.join(src_root, rel)
    dst = os.path.join(dst_root, rel)
    if not os.path.exists(src):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.isdir(src):
        shutil.copytree(src, dst)
    else:
        shutil.copy2(src, dst)


def trim(platform, src, dest):
    if os.path.exists(dest):
        shutil.rmtree(dest)
    os.makedirs(dest)
    exe = "zig.exe" if platform == "windows" else "zig"
    shutil.copy2(os.path.join(src, exe), os.path.join(dest, exe))
    shutil.copy2(os.path.join(src, "LICENSE"), os.path.join(dest, "LICENSE"))
    lib_src = os.path.join(src, "lib")
    lib_dst = os.path.join(dest, "lib")
    for rel in KEEP_COMMON:
        copy_entry(lib_src, lib_dst, rel)
    for rel in KEEP_LIBC[platform]:
        copy_entry(os.path.join(lib_src, "libc"), os.path.join(lib_dst, "libc"), rel)
    # Licence texts that ship inside lib/libc (mingw-w64, glibc).
    for dirpath, _, files in os.walk(os.path.join(lib_src, "libc")):
        for f in files:
            if f.upper().startswith(("COPYING", "LICENSE")):
                rel = os.path.relpath(os.path.join(dirpath, f), lib_src)
                copy_entry(lib_src, lib_dst, rel)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--platform", choices=sorted(RELEASES), required=True)
    ap.add_argument("--dest", required=True)
    ap.add_argument("--archive", help="an already downloaded release archive")
    args = ap.parse_args()
    name, data = fetch(args.platform, args.archive)
    with tempfile.TemporaryDirectory() as tmp:
        src = extract(name, data, tmp)
        trim(args.platform, src, args.dest)
    with open(os.path.join(args.dest, "VERSION"), "w") as f:
        f.write("zig %s %s\n" % (VERSION, RELEASES[args.platform][1]))
    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(args.dest) for f in fs)
    print("%s: zig %s for %s, %.0f MB" % (args.dest, VERSION, args.platform, total / 1e6))


if __name__ == "__main__":
    main()
