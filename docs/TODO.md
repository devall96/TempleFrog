# TempleFrog TODO

Feature backlog collected 2026-11-15. Roughly in priority order within each section.
Quick wins first: language picker, fast boot, rumble. Then the flagships: widescreen, enhanced audio.

## Rendering (notes from discussion)

- [x] Affine use: none of the 5,058 BIOS calls on the perf route (boot, menus, level 1) is an affine set-up, and the
      internal-resolution row is not exposed. The affine-smoothing filter's description already says it only smooths
      rotated/zoomed graphics.
- [ ] Re-check affine use in bosses and world transitions once a route reaches them.
- [x] The filters that actually matter: integer scale with nearest, sharp bilinear for non-integer fullscreen
      (native-view bug fixed), HQ2x/3x/4x, scanlines, LCD grid and ghosting, colour correction (all in the in-game
      menu).

## Audio

- [x] Read Frogger's GAX mix rate from the ROM: 8192 Hz, 8 voices, Timer1 chunks of 128 samples
      (docs/GAX_HQ_MIXER_RESEARCH.md).
- [x] Tier 1 "Original": faithful GAX output through the emulated FIFO, cleanly resampled (Enhanced audio off).
- [x] Tier 2 "Enhanced audio" (now the default): a native mixer in float (src/gax_hq.cpp) with windowed-sinc
      resampling, gain ramps, note fades, sample de-quantisation and a look-ahead limiter.
- [x] Enhanced audio in other scenes: 414 s of title plus attract demos (level 1 variants, a jungle-temple world, a
      lava-cave world) with TEMPLEFROG_GAX_VERIFY: 0% fallback, 98.0% of chunks bit-exact, 0.05% of bytes differ.
- [ ] Enhanced audio in bosses and the Temple world (no scripted route reaches them yet).
- [x] Echo: not used by this game. The setup at 0x08020388 never writes the FX delay (0x03000004), so the engine
      sizes the echo buffer at 0 bytes and the echo bus can never switch on. The fallback stays as a safety net.
- [x] Tone channels (Enhanced audio only): squares and noise output their average over each output sample. Error
      against an ideal band-limited square drops by 12-14 dB (GBARECOMP_PSG_SMOOTH=0 to compare). They only play
      sound effects here (47 notes in 45 s of demos).
- [ ] Full BLEP for the tone channels and averaging for the wave channel, if anyone can still hear aliasing.
- [x] Audio output at 48 kHz by default; the launcher's Sound > Sample rate row now really sets it.
- [ ] Tier 3 (optional, outside the original): replacement or remastered music packs through gbarecomp's mod audio
      (docs/MOD_AUDIO.md).

## Play quality

- [x] Blurred side bars (Graphics > Side bars, on by default): letterbox and pillarbox space shows a soft glow of the
      game instead of flat black. Presentation only.
- [ ] Widescreen / adaptive view: a real wider view of each level. Findings and the phased plan are in
      docs/WIDESCREEN_RESEARCH.md (the level BG ring is 32 tiles; the HUD and sprites assume 240 px).
- [ ] Frame-time polish: pacing on high-refresh/VRR displays and low input latency. Run tools/perf/run_suite.ps1
      -SaveBaseline first (it opens windows for about 6 minutes, so ask the user before taking the screen).
- [x] Remappable controls and analog-stick movement: already in gbarecomp (launcher bindings, stick-to-d-pad with a
      deadzone).
- [x] Controller glyphs: the in-game menu's prompts follow the last-used controls (keyboard, Xbox/generic, PlayStation
      with drawn shapes, Nintendo); launcher binding chips use the pad's printed names (LT/L2/ZL). Esc now resumes from
      any menu page and Backspace steps back.
- [ ] Controller UI bugs the user saw while testing (2026-11-15, details to collect): navigating the launcher or
      in-game menu with a pad.
- [ ] Glyphs: check with real PlayStation and Switch pads (tested by pinning the family, no physical pads).
- [x] Rumble (Game > Rumble, off by default): deaths, 1-ups, elements, coins, read from the HUD run-state at
      0x030035E0 (src/game_policy.cpp). The game has no health, so a hit is a death. gbarecomp now rumbles the game
      controller itself (SDL_GameControllerRumble).
- [ ] Rumble: feel-test with a controller. No event has fired in a scripted test yet: blind routes don't get Frogger
      past level 1's bushes. TEMPLEFROG_POLICY_TRACE=<key trace> plus TEMPLEFROG_RUMBLE_LOG=1 logs the events.

## Convenience

- [x] Save states, rewind (30 s), fast-forward: now in the overlay (they were not exposed before). Checked in level 1
      with GBARECOMP_ASSIST_SCRIPT. Fixed a gbarecomp pacer bug that froze for 268 ms on every load and rewind step.
- [x] Fast boot (System > Fast boot, on by default): skips the BIOS intro, then runs the Konami, "presents" and MusyX
      screens (which ignore the buttons) at turbo. Title in about 1.5 s instead of about 13 s. Also fixed gbarecomp's
      BIOS skip, which left the affine matrices at 0 and blanked the title screen.
- [x] Pause automatically when the window loses focus (System > Pause when unfocused, on by default).
- [x] One save folder: the battery save and the save states (saves/states, was next to the ROM) all live in saves/,
      ready for Steam Cloud or Syncthing. Settings stay next to the exe.

## Languages

- [x] The launcher, the in-game menu and TempleFrog's rows in English, French, German, Spanish and Italian, following
      Game > Language (system language on "Ask every time"). Translations: tools/i18n/translations.py.
- [ ] Native-speaker review of the French, German, Spanish and Italian text (written by Claude, checked on screen only).
- [ ] Still English: the "GAME BOY ADVANCE" subtitle, "Version", keyboard key names, and the netplay pages (not shown
      in TempleFrog). In German the save file name shortens to "..." next to the wider buttons.

## Content and extras

- [x] Language (Game > Language): the game asks every time you press Start; pick one and TempleFrog answers that
      screen with key presses (mode 27, cursor 0x03003494). Checked for English, Deutsch, Italiano and "Ask every time".
- [ ] Language and Rumble rows: check them in the real overlay (the scripted check could not focus the window).
- [ ] Sound test / music player in the menu.
- [x] Cheats: Mods > Cheats > Infinite lives (mods/preloaded/packages/templefrog.cheats), checked in level 1.
- [ ] More cheats: all levels (gGameStuff.unlockedWorlds at 0x0300533C needs its bit layout), coins (the saved
      coin total gates the Temple, so check what a forced count does to the save first).
- [ ] Achievements (later): our own offline set, not RetroAchievements. Unlock pop-ups and a list in the in-game menu,
      saved locally, working with every feature. Needs the game's level-clear and boss-defeat state in RAM
      (gGameStuff.unlockedWorlds at 0x0300533C, the HUD run-state at 0x030035E0). For reference only: RA has a
      26-achievement set by Layton (2023) covering bosses, the Temple, a deathless final boss and per-level
      collectors; write our own list rather than copying theirs.
- [x] Speedrun timer (Game > Speedrun timer): level time, total in-level time and the last split, counted in game
      frames, drawn bottom-right by the host overlay.
- [ ] Speedrun extras: a full split list, a reset rule, and telling level clears from quits (needs the clear flag).

## Visual

- [x] Colour-correction presets: already in Graphics > Colour correction (vivid, GBA unlit/frontlit, GBA SP, classic).
- [ ] CRT/LCD shader presets.
- [ ] Bezel/border art in fullscreen, generated from the player's ROM after verification (never ship game art).
- [x] Display modes: windowed / borderless / exclusive fullscreen already in gbarecomp.
- [ ] Per-monitor scaling check on a mixed-DPI setup.

## Distribution

- [ ] Steam Deck, Linux and Android builds (gbarecomp supports them; keep platform code behind #ifdefs).
- [ ] Auto-updater (like Savage Wind's).
