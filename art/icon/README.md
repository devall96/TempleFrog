# TempleFrog icon

Original artwork for TempleFrog: a cute green tree frog peeking over a carved sandstone temple arch, in colours
taken from the game's palette. It is not Konami's Frogger character and contains no game art. It is safe to ship
with the port, unlike anything extracted from the ROM.

- `templefrog_icon_source.jpg`: the source, generated with Google Gemini (2026-11-15) from a prompt describing an
  original frog-and-temple icon. The image carries Google's invisible SynthID marker.
- `templefrog_1024.png` … `templefrog_16.png`: the tile cut out onto transparency with anti-aliased rounded corners
  and a 3% margin.
- `templefrog.ico`: Windows icon with 16, 24, 32, 48, 64, 128 and 256 px images. It is embedded in `TempleFrog.exe`
  (`src/templefrog.rc`) and used for shortcuts.
- `brand_game.tga`: 128 px, 32-bit, the launcher header mark.

The build copies `templefrog.ico`, `templefrog_256.png` (as `icon.png`) and `brand_game.tga` into
`<exe dir>/assets/img/`.
