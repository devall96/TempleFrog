# TempleFrog's AppImage build machine: AlmaLinux 8 (glibc 2.28) with GCC 13.
# An AppImage runs on systems whose glibc is at least the build machine's, so
# this covers every SteamOS 3 (Steam Deck), Ubuntu 20.04+, Debian 10+,
# Fedora 29+ and current Arch. Used by tools/release_appimage.sh.
FROM docker.io/library/almalinux:8

RUN dnf install -y dnf-plugins-core epel-release \
 && dnf config-manager --set-enabled powertools \
 && dnf install -y \
        gcc-toolset-13-gcc gcc-toolset-13-gcc-c++ gcc-toolset-13-binutils \
        cmake ninja-build git python3 file findutils diffutils \
        mesa-libGL-devel mesa-libEGL-devel mesa-libgbm-devel libdrm-devel \
        libX11-devel libXext-devel libXcursor-devel libXi-devel libXfixes-devel \
        libXrandr-devel libXScrnSaver-devel libxkbcommon-devel \
        wayland-devel wayland-protocols-devel \
        pulseaudio-libs-devel alsa-lib-devel pipewire-devel \
        systemd-devel dbus-devel ibus-devel \
 && dnf clean all

ENV PATH=/opt/rh/gcc-toolset-13/root/usr/bin:$PATH \
    CC=gcc CXX=g++
