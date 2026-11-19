TempleFrog
==========

A fan-made PC port of Frogger's Adventures: Temple of the Frog (Game Boy Advance), built as a static
recompilation with gbarecomp. Made with AI (Claude). Not affiliated with Konami or Nintendo.

What you need (not included, never will be)
-------------------------------------------
- Your own Frogger's Adventures: Temple of the Frog (USA) ROM.
  SHA-1 7b4c27009198df18555e63fb5dcad223eaf09815
- Your own Game Boy Advance BIOS dump.
  SHA-1 300c20df6731a33952ded8c436f7f186d25d3492

Getting started
---------------
1. Make the AppImage executable (chmod +x TempleFrog-*.AppImage, or Properties > Permissions
   in your file manager) and run it.
2. Pick your ROM (Play > Browse For ROM) and BIOS (Settings > System > BIOS) in the launcher,
   or put them in ~/.local/share/TempleFrog/roms/frog_us.gba and .../bios/gba_bios.bin.
3. Press PLAY.

The first start builds the game from your ROM and BIOS (a minute or two, once). The AppImage holds no
code or data from the game: its game kit (TempleFrog's recompiler and the Zig compiler) makes it on your
machine, into ~/.local/share/TempleFrog/native.

TempleFrog runs from ~/.local/share/TempleFrog (or $TEMPLEFROG_HOME if you set it): the
AppImage copies the program there on first start and after an update. For a portable setup, put an
empty portable.txt next to the AppImage: everything then lives in TempleFrog-data beside it.

In game, Esc (or L3 / R3, or Select + Start on a controller) opens the menu: graphics, audio, save states,
rewind, language, rumble, the speedrun timer and more.

Your saves and save states are in ~/.local/share/TempleFrog/saves. Your settings are in config.ini
and templefrog.ini in the same folder. Delete that folder to start over.

Help, updates and the source: https://github.com/devall96/TempleFrog
Licences: LICENSE files in the licenses folder, and THIRD_PARTY_NOTICES.md. The program as a whole may
only be used and shared for non-commercial purposes (gbarecomp is PolyForm Noncommercial 1.0.0).
