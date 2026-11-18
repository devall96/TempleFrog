## TempleFrog 0.1.1

> [!TIP]
> ### ⬇️ Download: this is the only file you need
> **Windows:** [**TempleFrog-0.1.1-windows-x64.zip**](https://github.com/devall96/TempleFrog/releases/download/v0.1.1/TempleFrog-0.1.1-windows-x64.zip). Unzip it anywhere you can write to, add your ROM and BIOS, run `TempleFrog.exe`. Updating from 0.1.0: unzip over your old folder; your saves and settings stay.

**TempleFrog now works with [RecompLauncher](https://recomplauncher.com).** Install it there, pick your ROM, and press Play: the launcher checks your copy against the verified No-Intro dump, starts the game straight away, and shows TempleFrog's settings on the game's page.

### 🚀 RecompLauncher
- **Easy Setup**: pick your USA ROM (the `.gba`, or a `.zip` with it inside) and it's checked and ready to play.
- **Settings on the game's page**: window, size, filters, side bars, colour correction, scanlines, LCD grid, enhanced audio, volume, fast boot, pause when unfocused, language, rumble and the speedrun timer.
- **Big Picture mode** opens TempleFrog full screen, with button prompts for your controller from the first frame.
- **Clear messages** when something's wrong: a missing ROM, a zip without one, the European release, another file, or no BIOS yet.
- Saves and logs are where the launcher looks, so backups, cloud saves and crash reports pick them up.

### 🛠️ Also new
- `TempleFrog.exe --game <your ROM>` starts straight into the game with your saved settings (for scripts and shortcuts too). It takes zipped ROMs.
- A log of each run in `logs\templefrog.log` (the last five are kept), for bug reports.

### Known limits
- The BIOS can't be picked in RecompLauncher yet: put yours in `bios\gba_bios.bin` (the game page's Game folder button opens it), or pick it when TempleFrog asks.
- TempleFrog's own launcher still picks `.gba` files only; zipped ROMs work through `--game` and RecompLauncher.

> [!IMPORTANT]
> **Made with AI.** Written by Claude (Anthropic) through Claude Code, directed and tested by [devall96](https://github.com/devall96). Bring your own ROM and BIOS: no game files are included. Not affiliated with Konami or Nintendo. Non-commercial use only (gbarecomp is PolyForm Noncommercial 1.0.0).
