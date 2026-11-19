#!/usr/bin/env bash
# Build TempleFrog's Linux AppImage (x86_64):
#
#   tools/package_appimage.sh <version>
#
# Needs a C/C++ toolchain, cmake, ninja, python3, the X11 / Wayland /
# PulseAudio / ALSA / udev / libdecor development headers (for SDL's
# runtime-loaded backends) and appimagetool (in $APPIMAGETOOL, on PATH, or at
# tools/appimagetool-x86_64.AppImage). No ROM: the AppImage carries no code made
# from the game. The player's ROM and BIOS are recompiled and compiled into game
# modules on their machine with the game kit packed here (gbarecomp/docs/GAME_MODULES.md),
# so this runs anywhere, GitHub Actions included.
#
# The AppImage needs a glibc at least as new as the build machine's: releases are
# built by tools/release_appimage.sh in an AlmaLinux 8 container (glibc 2.28).
#
# Writes out/release/TempleFrog-<version>-linux-x86_64.AppImage, SHA256SUMS-linux.txt
# and BUILDINFO-linux.json. Like package_release.ps1, it never packages a ROM, BIOS,
# save, the player's settings, images made from the ROM or code recompiled from the
# ROM or BIOS; the audit (tools/audit_package.py) fails the build if any of those,
# or a personal path, gets in.
set -euo pipefail

version="${1:?usage: tools/package_appimage.sh <version>}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="${APPIMAGE_WORK:-$root/build-appimage}"
sdl_src="$root/gbarecomp/platform/android/third_party/SDL"
sdl_prefix="$work/sdl"
name="TempleFrog-$version-linux-x86_64"
out="$root/out/release"
appdir="$out/$name.AppDir"
payload="$appdir/usr/lib/templefrog"
jobs="$(nproc)"

appimagetool="${APPIMAGETOOL:-$(command -v appimagetool || true)}"
[[ -z "$appimagetool" && -x "$root/tools/appimagetool-x86_64.AppImage" ]] &&
    appimagetool="$root/tools/appimagetool-x86_64.AppImage"
[[ -n "$appimagetool" ]] || { echo "appimagetool not found (set APPIMAGETOOL)" >&2; exit 1; }

# Source paths mapped to "." so no local path ends up in a binary.
map="-ffile-prefix-map=$root=."

# ---- SDL2 from gbarecomp's pinned source ------------------------------------
# Built as a shared library whose video/audio backends (X11, Wayland, PulseAudio,
# PipeWire, ALSA, ...) are dlopen()ed from the player's system: it links only
# libc/libm, so the AppImage doesn't carry (or clash with) desktop libraries.
if [[ ! -f "$sdl_prefix/lib/libSDL2-2.0.so.0" ]]; then
    cmake -S "$sdl_src" -B "$work/sdl-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$sdl_prefix" -DCMAKE_INSTALL_LIBDIR=lib -DSDL_SHARED=ON -DSDL_STATIC=OFF \
        -DSDL_TEST=OFF -DSDL_RPATH=OFF -DCMAKE_C_FLAGS="$map"
    cmake --build "$work/sdl-build" -j "$jobs"
    cmake --install "$work/sdl-build"
fi

# ---- the compiler the game kit carries ------------------------------------------
zig_dir="${TEMPLEFROG_ZIG_DIR:-$root/tools/toolchain-cache/zig-linux}"
[[ -x "$zig_dir/zig" ]] || python3 "$root/tools/zig_toolchain.py" --platform linux --dest "$zig_dir"

# ---- TempleFrog ----------------------------------------------------------------
# $ORIGIN/lib finds the bundled SDL next to the program; no build-tree RPATH.
cmake -S "$root" -B "$work/game" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$sdl_prefix" -DSDL2_DIR="$sdl_prefix/lib/cmake/SDL2" \
    -DOpenGL_GL_PREFERENCE=LEGACY \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_INSTALL_RPATH='$ORIGIN/lib' \
    -DCMAKE_EXE_LINKER_FLAGS="${TEMPLEFROG_LDFLAGS:-}" \
    -DCMAKE_C_FLAGS="$map" -DCMAKE_CXX_FLAGS="$map" \
    -DTEMPLEFROG_GAME_MODULES=ON -DGBARECOMP_EMBED_SRC_DIR=OFF -DTEMPLEFROG_ZIG_DIR="$zig_dir"
cmake --build "$work/game" --target TempleFrog templefrog_gamekit -j "$jobs"
build="$work/game"

# ---- AppDir --------------------------------------------------------------------
rm -rf "$appdir"
mkdir -p "$payload/lib" "$appdir/usr/share/applications" \
         "$appdir/usr/share/icons/hicolor/256x256/apps" "$appdir/usr/share/metainfo"

install -m755 "$build/TempleFrog" "$payload/"
strip --strip-unneeded "$payload/TempleFrog"
[[ -f "$build/gbarecomp-hqx.so" ]] && install -m755 "$build/gbarecomp-hqx.so" "$payload/"
cp -L "$sdl_prefix/lib/libSDL2-2.0.so.0" "$payload/lib/"
strip --strip-unneeded "$payload/lib/libSDL2-2.0.so.0"
# libGL.so.1 is on every desktop; GLVND's libOpenGL.so.0 often isn't.
if readelf -d "$payload/TempleFrog" | grep -q 'libOpenGL.so'; then
    echo "TempleFrog links libOpenGL.so.0; it must use libGL.so.1" >&2; exit 1
fi
if ! readelf -d "$payload/TempleFrog" | grep -qE 'R(UN)?PATH.*\$ORIGIN/lib' ||
   ! ldd "$payload/TempleFrog" | grep -q "libSDL2-2.0.so.0 => $payload/lib/"; then
    echo "TempleFrog doesn't load the bundled SDL2 from \$ORIGIN/lib" >&2; exit 1
fi

# Art and fonts (never the box art or backdrop: those are made from the player's ROM).
cp -R "$build/assets" "$payload/"
rm -f "$payload/assets/img/boxart.tga" "$payload/assets/img/backdrop.tga"
[[ -d "$build/third-party" ]] && cp -R "$build/third-party" "$payload/"
# Preloaded mod packages only (not the player's selection).
mkdir -p "$payload/mods"
cp -R "$build/mods/packages" "$payload/mods/"
# The game kit: recompiler, module SDK and compiler. AppRun runs it from the image.
cp -R "$build/gamekit" "$payload/gamekit"
strip --strip-unneeded "$payload/gamekit/gba_recompile"

# game.toml with the release-relative BIOS path.
sed 's|path = "gbarecomp/bios/gba_bios.bin"|path = "bios/gba_bios.bin"|' "$root/game.toml" > "$payload/game.toml"
mkdir -p "$payload/roms" "$payload/bios"
printf "Copy your own Frogger's Adventures: Temple of the Frog (USA) ROM here as frog_us.gba,\nor pick it in the launcher (Play > Browse For ROM). SHA-1: 7b4c27009198df18555e63fb5dcad223eaf09815\n" \
    > "$payload/roms/PUT YOUR ROM HERE.txt"
printf "Copy your own Game Boy Advance BIOS dump here as gba_bios.bin,\nor pick it in the launcher (Settings > System > BIOS). SHA-1: 300c20df6731a33952ded8c436f7f186d25d3492\n" \
    > "$payload/bios/PUT YOUR BIOS HERE.txt"

# Licences.
lic="$payload/licenses"
mkdir -p "$lic"
cp "$root/LICENSE" "$lic/TempleFrog-MIT.txt"
cp "$root/THIRD_PARTY_NOTICES.md" "$payload/"
cp "$root/gbarecomp/LICENSE" "$lic/gbarecomp-PolyForm-Noncommercial-1.0.0.txt"
cp "$root/recomp-ui/LICENSE" "$lic/recomp-ui-MIT.txt"
cp "$sdl_src/LICENSE.txt" "$lic/SDL2-zlib.txt"
cp "$root/third_party/fonts/Chewy/LICENSE.txt" "$lic/Chewy-Apache-2.0.txt"
cp "$root/third_party/fonts/Fredoka/OFL.txt" "$lic/Fredoka-OFL-1.1.txt"
cp "$root/recomp-ui/assets/common/fonts/NOTICE.md" "$lic/Launcher-fonts-NOTICE.md"
[[ -f "$root/recomp-ui/src/third_party/imgui/LICENSE.txt" ]] &&
    cp "$root/recomp-ui/src/third_party/imgui/LICENSE.txt" "$lic/DearImGui-MIT.txt"
cp "$root/docs/README-release-linux.txt" "$payload/README.txt"
cp "$root/recomplauncher.json" "$payload/"
python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$payload/recomplauncher.json"

# Build stamp: AppRun refreshes the player's copy when this changes.
printf '%s-%s\n' "$version" "$(sha256sum "$payload/TempleFrog" | cut -c1-16)" > "$payload/.build-stamp"

# AppImage metadata.
install -m755 "$root/packaging/linux/AppRun" "$appdir/AppRun"
cp "$root/packaging/linux/templefrog.desktop" "$appdir/templefrog.desktop"
echo "X-AppImage-Version=$version" >> "$appdir/templefrog.desktop"
# RecompLauncher reads /recomplauncher.json inside the AppImage.
cp "$root/recomplauncher.json" "$appdir/recomplauncher.json"
cp "$root/packaging/linux/templefrog.desktop" "$appdir/usr/share/applications/"
cp "$root/art/icon/templefrog_256.png" "$appdir/templefrog.png"
cp "$root/art/icon/templefrog_256.png" "$appdir/usr/share/icons/hicolor/256x256/apps/templefrog.png"
ln -s templefrog.png "$appdir/.DirIcon"

# ---- audit ----------------------------------------------------------------------
# ROM/BIOS files, saves, settings, game code (recompiler-made names; with the
# ROM and BIOS at hand, slices of their bytes too) and personal strings. The
# build machine's home and user name come from the host for container builds.
audit_home="${AUDIT_HOME:-$HOME}"; audit_user="${AUDIT_USER:-${USER:-$(id -un)}}"
audit=(python3 "$root/tools/audit_package.py" "$appdir"
       --needle proton.me --needle "Claude Work" --needle devall96@ --needle "/home/$audit_user/")
[[ ${#audit_home} -gt 1 ]] && audit+=(--needle "$audit_home/")
[[ -f "$root/roms/frog_us.gba" ]] && audit+=(--rom "$root/roms/frog_us.gba")
[[ -f "$root/gbarecomp/bios/gba_bios.bin" ]] && audit+=(--bios "$root/gbarecomp/bios/gba_bios.bin")
"${audit[@]}"

# ---- AppImage, sums, build info --------------------------------------------------
image="$out/$name.AppImage"
rm -f "$image"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$appimagetool" --no-appstream "$appdir" "$image"

commit() { git -C "$1" rev-parse HEAD; }
glibc="$(objdump -T "$payload/TempleFrog" "$payload/lib/libSDL2-2.0.so.0" |
         grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)"
cat > "$out/BUILDINFO-linux.json" <<JSON
{
  "name": "TempleFrog",
  "version": "$version",
  "platform": "linux-x86_64",
  "built_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "templefrog_commit": "$(commit "$root")",
  "gbarecomp_commit": "$(commit "$root/gbarecomp")",
  "recomp_ui_commit": "$(commit "$root/recomp-ui")",
  "rom_sha1": "7b4c27009198df18555e63fb5dcad223eaf09815",
  "bios_sha1": "300c20df6731a33952ded8c436f7f186d25d3492",
  "min_glibc": "${glibc#GLIBC_}",
  "toolchain": "$(${CXX:-c++} --version | head -1), CMake Release"
}
JSON
(cd "$out" && sha256sum "$name.AppImage" BUILDINFO-linux.json > SHA256SUMS-linux.txt)
ls -la "$image"
echo "needs glibc >= ${glibc#GLIBC_}"
