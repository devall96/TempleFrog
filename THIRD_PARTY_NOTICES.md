# Third-party notices

TempleFrog's own code is MIT-licensed (see `LICENSE`). It is built on, and its releases include, the following work.
Release builds carry each licence text in the `licenses/` folder.

| Component | Used for | Licence |
| --- | --- | --- |
| [gbarecomp](https://github.com/mstan/gbarecomp) by Matthew Stanley, with TempleFrog's changes in [devall96/gbarecomp](https://github.com/devall96/gbarecomp) | The static recompiler, runtime, PPU, audio and host window | [PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0): **non-commercial use only** |
| [recomp-ui](https://github.com/mstan/recomp-ui) by Matthew Stanley, with TempleFrog's changes in [devall96/recomp-ui](https://github.com/devall96/recomp-ui) | The launcher and in-game menu | MIT |
| [SDL2](https://libsdl.org) | Window, input, audio | zlib |
| [hqx](https://github.com/grom358/hqx) (`gbarecomp-hqx.dll`, source included in `third-party/hqx`) | HQ2x/3x/4x pixel smoothing | LGPL-2.1 |
| MinGW-w64 runtime (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`) | C++ runtime | GPL-3.0 with the GCC Runtime Library Exception; winpthreads: MIT/BSD |
| [Dear ImGui](https://github.com/ocornut/imgui) (inside recomp-ui) | UI drawing | MIT |
| [Chewy](https://fonts.google.com/specimen/Chewy) by Sideshow | Titles | Apache 2.0 |
| [Fredoka](https://github.com/hafontia/Fredoka-One) by Milena Brandão | Menu text | SIL OFL 1.1 |
| LatoLatin by Łukasz Dziedzic, Noto Sans Symbols 2 by the Noto Project Authors | Fallback text and symbols | SIL OFL 1.1 |
| [OpenMoji](https://openmoji.org) | Emoji fallback | CC BY-SA 4.0 |
| [Zig](https://ziglang.org) 0.17.1 (`gamekit/zig`), with the [LLVM/Clang](https://llvm.org) it is built on | Compiles the game from your ROM and BIOS on your PC (the game kit) | MIT (Zig); Apache 2.0 with LLVM Exception (LLVM, Clang, libc++ headers) |
| [mingw-w64](https://www.mingw-w64.org) headers and C runtime sources (Windows kit), glibc headers and stubs (Linux kit), shipped inside Zig | The C library the game is compiled against | mingw-w64: ZPL / public domain (see `gamekit/zig/lib/libc/mingw/COPYING`); glibc: LGPL-2.1 (see `gamekit/zig/lib/libc/glibc/LICENSES`) |

The TempleFrog icon is original art made for this project with Google Gemini (see `art/icon/README.md`). It is not
Konami's character and contains no game art.

Research that helped: [JRickey/frog-adv-temple-decomp](https://github.com/JRickey/frog-adv-temple-decomp) (addresses
for the game mode, the HUD run-state and the room grid), and mstan's
[Mega Man Zero](https://github.com/mstan/megamanzerorecomp) and
[Minish Cap](https://github.com/mstan/MinishCapRecomp) recompilations as reference ports.

*Frogger's Adventures: Temple of the Frog* and *Frogger* are © Konami. Game Boy Advance is a trademark of Nintendo.
TempleFrog is not affiliated with or endorsed by Konami or Nintendo.
