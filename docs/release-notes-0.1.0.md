## TempleFrog 0.1.0

> [!TIP]
> ### ⬇️ Download: this is the only file you need
> **Windows:** [**TempleFrog-0.1.0-windows-x64.zip**](https://github.com/devall96/TempleFrog/releases/download/v0.1.0/TempleFrog-0.1.0-windows-x64.zip). Unzip it anywhere you can write to, add your ROM and BIOS, run `TempleFrog.exe`.
>
> `SHA256SUMS.txt` and `BUILDINFO.json` are there to check the download and see exactly what it was built from.

**The first release: *Frogger's Adventures: Temple of the Frog* on PC.** TempleFrog is a static recompilation of the Game Boy Advance game: its code translated ahead of time into a native Windows program with [gbarecomp](https://github.com/mstan/gbarecomp), plus a launcher, an in-game menu and PC comforts.

> [!CAUTION]
> **Bring your own ROM and BIOS.** No game files are included. You need *Frogger's Adventures: Temple of the Frog* (USA, SHA-1 `7b4c27009198df18555e63fb5dcad223eaf09815`) and a Game Boy Advance BIOS dumped from your own hardware (SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`).

### 🎵 Sound
- **Enhanced audio**, on by default: a new mixer for the game's sound engine in floating point at your output rate, with clean resampling, smooth note starts and stops, cleaned-up instrument samples and a look-ahead limiter. No hiss, crunch or clicks. The original sound is one switch away.
- **48 kHz output** by default; pick 32, 44.1 or 48 kHz in the launcher.

### 🖼️ Picture
- Integer scaling, sharp bilinear, linear, **HQ2x/3x/4x**, scanlines, LCD grid and ghosting, colour correction (GBA, GBA SP, classic, vivid).
- **Blurred side bars**: wide or tall windows show a soft glow of the game instead of black.
- Windowed, borderless or exclusive fullscreen.

### ⏩ Convenience
- **Fast boot**: the title screen in about 1.5 s instead of about 13 s.
- **Save states, rewind (30 s) and fast-forward** in the in-game menu.
- **Pause when the window loses focus.**
- Save and save states together in one `saves` folder.

### 🎮 Controls
- Keyboard or controller, remappable, with analog stick support.
- **Button prompts that match your controller**: Xbox, PlayStation or Switch style, or keyboard keys.
- **Rumble** (optional) on lost lives, 1-ups, elements and coins.

### 🌍 Languages
- Launcher and menus in **English, French, German, Spanish and Italian**.
- **Game > Language** answers the game's language screen for you and switches the menus too.

### ✨ Extras
- **Speedrun timer** (level, total and last split, in game frames).
- **Infinite lives** cheat on the Mods page (off by default).
- Desktop shortcut and Start menu entry from the launcher.

### Known limits
- Tested on one Windows PC. Level 1 and the attract demos of several worlds were checked closely; later worlds, bosses and the Temple haven't been played through on TempleFrog yet.
- Windows only for now. Linux, Steam Deck and Android builds are planned.
- Widescreen is not in this release (the plan is in `docs/WIDESCREEN_RESEARCH.md`).
- Some controller navigation quirks in the menus.
- The French, German, Spanish and Italian text was written by Claude and checked on screen, not by native speakers yet.

> [!IMPORTANT]
> **Made with AI.** TempleFrog's code, its changes to gbarecomp and recomp-ui, the art and these notes were written or made by Claude (Anthropic) through Claude Code, directed and tested by [devall96](https://github.com/devall96). Not affiliated with Konami or Nintendo. TempleFrog as a whole is for non-commercial use only (gbarecomp is PolyForm Noncommercial 1.0.0).
