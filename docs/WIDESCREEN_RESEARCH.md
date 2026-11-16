# Widescreen research

Status 2026-11-15: findings and a plan. Nothing is offered to players yet.

## What a wider view shows today

Test knob: `TEMPLEFROG_WS_EXPERIMENT=1` lets `--view-width` go up to 284 (16:9 at 160 lines). gbarecomp's
extended view then renders 22 extra columns on each side straight from the game's own VRAM.

Frames from level 1 and the attract demos (jungle-temple and lava-cave levels) at 284 wide:

- **Background wraps.** Both level layers (BG0CNT `0x1C03`, BG1CNT `0x1D02`) use 256x256 maps, a 32-tile ring.
  The visible 240 px are 30 tiles, so the game streams only about 2 tiles past each edge. At 284 px the margins
  show the ring wrapping: the far side of the map, or columns not drawn yet.
- **HUD in the margins.** The lives and coin counters live on a BG layer at fixed screen positions, so their
  neighbouring tiles leak into the margins.
- **Sprites.** Objects past x = 240 are expected to be culled by the game (not yet confirmed in code).
- **Scrolling.** Level 1 scrolls vertically only (HOFS 0, VOFS up to 0x500). Other worlds also scroll sideways
  (HOFS covers the whole 0-255 ring, VOFS up to 1040 in the demos).
- **Title and menus** are fixed 240-wide pictures; pillarbox those.

## Prior art (checked 2026-11-15)

Several gbarecomp-family ports already ship real widescreen:

- **Mega Man Zero** (mstan/megamanzerorecomp, `src/mmz_extended_view.cpp`, ~1,100 lines, up to 480 px, 288
  recommended). It runs the game's own background streamers (full-reload and incremental) as synthetic guest calls
  at shifted camera positions, with writes journaled and rolled back, into presentation-only margin maps that
  gbarecomp's PPU draws (`g_ws_authored_margin_layers`). Caches reset on state loads, full reloads and coordinate
  jumps. Twelve OAM clipping paths and three spawn-manager bounds are widened through reviewed ROM-literal overrides
  (`gba::g_rom_read32_override`). The BG0 HUD and boss gauge are anchored to the edges. Margins fail closed to black
  at camera bounds, outer map edges, inactive stage scenes and transitions (`g_ws_pillarbox*`).
- **Minish Cap** (MinishCapRecomp, adaptive up to 480 px). Margins come from the game's complete room-layer buffers in
  RAM rather than the 32x32 hardware ring. The HUD follows the corners; dialogue stays centred.
- **Final Fantasy Tactics Advance** (448 px) and **Buu's Fury** (480 px) recomps also have widescreen modes.
- Emulators (mGBA and others) have no general GBA widescreen hack; every port is game-specific.

Our gbarecomp already has every primitive Zero uses: `runtime_call_push_return` / `runtime_dispatch` /
`runtime_call_cancel_return` for synthetic calls, `BusWriteObserver`, `g_ws_authored_margin_layers`,
`g_ws_pillarbox*`, `g_rom_read32_override` and `RunOptions::extended_view_init`.

Frogger's fit:

- Frogger keeps the whole room in RAM like Minish Cap: a packed halfword grid at EWRAM `0x02030000` (5 x 4096-byte
  banks, row stride from `sSpriteAssetIndexTable` at `0x08306444`; decomp `src/engine/tile_grid_read.c`). That gives
  exact level bounds and a fallback source for the margins.
- The Zero route (call the game's own streamer at shifted positions) is the safer way to get pixel-exact margins,
  because it reuses the game's metatile-to-tile drawing instead of re-implementing it.

## What real widescreen needs

1. **Wider level rings.** Switch the level BGs to 512-wide maps (BGxCNT size bit) and make the column streamer fill
   64 columns, drawing the extra columns on both sides. Needs the level renderer and its streaming entry points
   (still assembly in JRickey's decomp; the HUD blitter `sub_080169C0` and the TileBlit helper are known).
2. **Camera.** Find the camera clamp at level edges. Where a level is narrower than the wide view, show a border
   instead of wrapped tiles.
3. **Sprites.** Find the on-screen test that culls objects outside 0..240 and widen it; place OAM in the extended
   coordinate space (gbarecomp's extended view supports x beyond 240).
4. **HUD.** Move the lives counter to the left edge and the coins to the right edge (the descriptors are
   `sFrogStatusBarFrames`, x/y/w/h per element), or keep the HUD in the centre 240.
5. **Per-scene policy.** `RunOptions::extended_view_frame` lets the game decide per frame: wide in levels, pillarbox
   on the title, menus, cut-scenes and the world map until each is checked.

Changes go in through reviewed game metadata and hooks, never by hand-editing generated code. With widescreen off,
the run must stay byte-identical.

## Phases (following Mega Man Zero's design)

1. Trace the level renderer: log BG map writes (screen blocks 28/29) with a bus write observer while walking level 1
   and a sideways level, and find the full-reload and incremental streamer functions, their state structs and the
   camera variable.
2. Wider ring plus streaming for one level, margins correct while scrolling (compare against a static full-map
   render of the level layout data).
3. Sprite culling and HUD placement.
4. Every world; pillarbox the scenes that cannot widen; frame-time suite; then offer it in Settings.

Interim option, presentation only: fill the side bars with a blurred, darkened stretch of the game image (ambient
pillarbox) so 16:9 screens are not black at the sides.
