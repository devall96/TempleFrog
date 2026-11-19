## TempleFrog 0.2.0

> [!TIP]
> ### ⬇️ Download: one file for your system
> **Windows:** [**TempleFrog-0.2.0-windows-x64.zip**](https://github.com/devall96/TempleFrog/releases/download/v0.2.0/TempleFrog-0.2.0-windows-x64.zip). Unzip it anywhere you can write to, add your ROM and BIOS, run `TempleFrog.exe`.
> **Linux and Steam Deck:** [**TempleFrog-0.2.0-linux-x86_64.AppImage**](https://github.com/devall96/TempleFrog/releases/download/v0.2.0/TempleFrog-0.2.0-linux-x86_64.AppImage). Make it executable and run it.
> Updating from 0.1: unzip over your old folder (or replace the AppImage); your saves and settings stay.

**TempleFrog's download no longer holds any of the game.** Earlier releases had the game's program, recompiled, built into `TempleFrog.exe`. From 0.2.0 on, TempleFrog builds the game from **your own ROM and BIOS, on your PC**, the first time you press Play, with the recompiler and compiler it now comes with. This takes a minute or two, once, behind a progress window. The game it makes plays exactly the same as 0.1.1, frame for frame.

### 🛠️ What changes for you
- **The first start builds the game** (a minute or two). After that TempleFrog starts as fast as before. It builds again only when an update changes the recompiler, or when you pick a different ROM or BIOS.
- **A bigger download** (about 95 MB): it now includes the game kit, TempleFrog's recompiler and the [Zig](https://ziglang.org) C++ compiler, in the `gamekit` folder. The game built from your files goes in the `native` folder.
- **RecompLauncher** builds the game on its setup screen ("Getting the extras"), right after it checks your ROM. Put your BIOS in the `bios` folder first and it builds that too; otherwise TempleFrog does it on the first start.
- **Linux builds now come from GitHub Actions**, checked on Ubuntu, Debian, AlmaLinux, Fedora and Arch for every release.

### Known limits
- TempleFrog needs a folder it can write to (not *Program Files*), as before: the game is built there.
- If the build fails, `native\build.log` says why; `TempleFrog.exe --kit-selftest` checks the compiler without your ROM. Please send both with a bug report.

> [!IMPORTANT]
> **Made with AI.** Written by Claude (Anthropic) through Claude Code, directed and tested by [devall96](https://github.com/devall96). Bring your own ROM and BIOS: no game files, and no code or data from the game, are included. Not affiliated with Konami or Nintendo. Non-commercial use only (gbarecomp is PolyForm Noncommercial 1.0.0).
