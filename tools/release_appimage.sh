#!/usr/bin/env bash
# The Linux release: build the AppImage inside the AlmaLinux 8 container
# (tools/appimage-builder.Containerfile, glibc 2.28) so it runs on old systems,
# Steam Deck included. Needs podman and appimagetool at
# tools/appimagetool-x86_64.AppImage; no ROM (the AppImage carries no game code,
# gbarecomp/docs/GAME_MODULES.md), so GitHub Actions runs it too (.github/workflows/linux.yml).
#
#   tools/release_appimage.sh <version>
set -euo pipefail
version="${1:?usage: tools/release_appimage.sh <version>}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image=templefrog-appimage-builder

podman build -t "$image" -f "$root/tools/appimage-builder.Containerfile" "$root/tools"
# A separate build tree, so the host's (newer-glibc) objects are never reused.
# libstdc++ is linked in: the player's system only needs glibc and libgcc_s.
podman run --rm --userns=keep-id -v "$root:/src" -w /src \
    -e APPIMAGE_WORK=/src/build-appimage-el8 \
    -e TEMPLEFROG_LDFLAGS="-static-libstdc++" \
    -e AUDIT_HOME="$HOME" -e AUDIT_USER="$(id -un)" \
    -e APPIMAGETOOL=/src/tools/appimagetool-x86_64.AppImage \
    "$image" tools/package_appimage.sh "$version"
