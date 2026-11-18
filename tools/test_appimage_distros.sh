#!/usr/bin/env bash
# Smoke-test the AppImage on older and other distros in podman containers.
#
#   tools/test_appimage_distros.sh <TempleFrog.AppImage> [rom] [bios]
#
# In each container: run the game headless to the title screen (frame 900) and
# compare the frame with every other distro's, then open the launcher on a
# virtual 1280x800 X display (the Steam Deck's size) and check it stays up for
# 10 s, which exercises SDL loading X11/OpenGL from that distro. The ROM and BIOS
# are mounted read-only and never leave this machine.
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
appimage="$(readlink -f "${1:?usage: tools/test_appimage_distros.sh <AppImage> [rom] [bios]}")"
rom="$(readlink -f "${2:-$root/roms/frog_us.gba}")"
bios="$(readlink -f "${3:-$root/gbarecomp/bios/gba_bios.bin}")"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

apt_deps='export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq libgl1 xvfb xauth >/dev/null'
dnf_deps='dnf install -y -q mesa-libGL mesa-dri-drivers xorg-x11-server-Xvfb xorg-x11-xauth which util-linux >/dev/null'
# Debian 11 has moved to the archive mirror.
deb11_deps="sed -i 's#deb.debian.org#archive.debian.org#g; /security/d' /etc/apt/sources.list; $apt_deps"
pac_deps='pacman -Sy --noconfirm --needed mesa xorg-server-xvfb xorg-xauth >/dev/null'

# image | deps | label
distros=(
  "docker.io/library/ubuntu:20.04|$apt_deps|Ubuntu 20.04 (glibc 2.31)"
  "docker.io/library/debian:11|$deb11_deps|Debian 11 (glibc 2.31, Steam Runtime 3 base)"
  "docker.io/library/ubuntu:22.04|$apt_deps|Ubuntu 22.04 (glibc 2.35)"
  "docker.io/library/almalinux:8|$dnf_deps|AlmaLinux 8 (glibc 2.28, the build base)"
  "docker.io/library/fedora:36|$dnf_deps|Fedora 36 (glibc 2.35)"
  "docker.io/library/fedora:latest|$dnf_deps|Fedora latest"
  "docker.io/library/archlinux:latest|$pac_deps|Arch latest (SteamOS 3 base)"
)
[[ -n "${DISTROS:-}" ]] && mapfile -t distros < <(printf '%s\n' "${distros[@]}" | grep -E "$DISTROS")

# Runs inside the container as root; /app is the AppImage, /out the results.
inner='
set -u
eval "$DEPS" || { echo "DEPS_FAILED"; exit 3; }
cp /app/TempleFrog.AppImage /tmp/tf.AppImage
export APPIMAGE_EXTRACT_AND_RUN=1 TEMPLEFROG_HOME=/tmp/home
mkdir -p /tmp/home/roms /tmp/home/bios
cp /rom /tmp/home/roms/frog_us.gba; cp /bios /tmp/home/bios/gba_bios.bin
ldd_out=$(cd /tmp && /tmp/tf.AppImage --appimage-extract >/dev/null 2>&1; ldd /tmp/squashfs-root/usr/lib/templefrog/TempleFrog 2>&1 | grep -i "not found")
[ -n "$ldd_out" ] && echo "MISSING_LIBS: $ldd_out"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 120 /tmp/tf.AppImage --no-window --frames 900 \
    --dump-bmp /out/frame.bmp --save-path /tmp/scratch.sav game.toml > /out/headless.log 2>&1
echo "HEADLESS_EXIT=$?"
grep -o "self_heal_coverage=[A-Z_]*" /out/headless.log
SDL_AUDIODRIVER=dummy xvfb-run -a -s "-screen 0 1280x800x24" timeout 10 /tmp/tf.AppImage > /out/gui.log 2>&1
echo "GUI_EXIT=$?"
'

printf '%-48s %-10s %-10s %-14s %s\n' DISTRO HEADLESS GUI COVERAGE FRAME
ref=""; fails=0
for entry in "${distros[@]}"; do
    IFS='|' read -r image deps label <<<"$entry"
    d="$out/$(echo "$image" | tr '/:' '__')"; mkdir -p "$d"
    podman run --rm -v "$appimage:/app/TempleFrog.AppImage:ro" -v "$rom:/rom:ro" -v "$bios:/bios:ro" \
        -v "$d:/out" -e DEPS="$deps" "$image" sh -c "$inner" > "$d/run.txt" 2>&1
    h=$(sed -n 's/^HEADLESS_EXIT=//p' "$d/run.txt"); g=$(sed -n 's/^GUI_EXIT=//p' "$d/run.txt")
    cov=$(grep -o 'FULLY_STATIC\|[A-Z_]*STATIC[A-Z_]*' "$d/run.txt" | head -1)
    frame="-"
    if [[ -f "$d/frame.bmp" ]]; then
        sum=$(sha1sum "$d/frame.bmp" | cut -c1-12); [[ -z "$ref" ]] && ref=$sum
        [[ "$sum" == "$ref" ]] && frame="match" || frame="DIFFERS"
    fi
    hs="FAIL"; [[ "$h" == 0 ]] && hs="ok"
    gs="FAIL"; [[ "$g" == 124 ]] && gs="ok (up)"   # 124: still running when timeout stopped it
    grep -q '^MISSING_LIBS' "$d/run.txt" && gs="$gs, libs!"
    printf '%-48s %-10s %-10s %-14s %s\n' "$label" "$hs" "$gs" "${cov:--}" "$frame"
    if [[ "$hs" != ok || "$g" != 124 || "$frame" != match ]]; then
        fails=$((fails + 1))
        echo "---- $label"; tail -15 "$d/run.txt"; tail -15 "$d/gui.log" 2>/dev/null; echo "----"
    fi
done
exit $((fails > 0))
