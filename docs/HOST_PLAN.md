# TempleFrog host plan

Reference port: mstan/DragonBallZLegacyofGokuIIRecomp (pins gbarecomp `cebb1e4e`, 2026-11-11).
Schema: `gbarecomp/docs/TOML_SCHEMA.md`. Perf tooling: `gbarecomp/docs/RUNTIME_PERFORMANCE.md`.

## Layout

```
TempleFrog/
  gbarecomp/            submodule, pinned (v0.4.1 = c1a5fda, or a newer main commit)
  game.toml             identity, program bounds, roots, save type
  src/main.cpp          fills gbarecomp::RunOptions, calls gbarecomp::run_game
  tools/regen.ps1       ROM SHA-1 gate, then gba_recompile --config game.toml --out generated
  generated/            gitignored except README.md
  roms/frog_us.gba      gitignored; user's own dump
  gbarecomp/bios/gba_bios.bin   gitignored; user's own dump, SHA-1 300c20df6731a33952ded8c436f7f186d25d3492
```

## Build

- Toolchain used by the ports: MSYS2 MINGW64 with cmake, ninja, gcc and SDL2. The runtime looks for SDL2 under `C:/msys64/mingw64`.
  MSVC is supported for the generated library (already compiles) and the CMake has `/STACK` handling.
- Link: `gbarecomp_runtime gbarecomp_debug gbarecomp_gba gbarecomp_armv4t` (MinGW start/end group).
- Shards compiled at `-O1 -g0`; 16 MB stack.
- Defines: `GBARECOMP_GAME_VERSION`, `GBARECOMP_DEFAULT_DEBUG_PORT` (pick an unused port), `GBARECOMP_WINDOW_TITLE="TempleFrog"`,
  `GBARECOMP_DEFAULT_GAME_CONFIG="game.toml"`.
- Generated BIOS: `gba_recompile --bios bios/gba_bios.bin --config bios/gba_bios.toml --out <GBARECOMP_GENERATED_BIOS_DIR>`.
  Without it, the build links only a placeholder stub.
- Launcher (`recomp-ui`): later.

## game.toml starting point

```toml
[identity]
sha1 = "7b4c27009198df18555e63fb5dcad223eaf09815"

[program]
load_address = 0x08000000
entry_pc = 0x08000000
speculative_literal_harvest = false
aot_scan_start = 0x080000C0
aot_scan_end   = 0x08036000   # half-open; end of code per the decomp
static_resume_all = true

[audio]
shadow = false                # GAX, not MP2K; shadow verifier is MP2K-only
```

Then: `[save]` (type to be confirmed from the ROM/decomp), `[[data_range]]` for 0x08036000..end once the 6 outliers near
0x081C0000 are explained, `[[code_copy]]` + ARM `[[extra_func]]` for any IWRAM code (GAX mixer, IRQ handler).
Use the decomp's `linker.ld`, `frog_us_baserom.labels.toml` and `docs/memory-map.md` as evidence for roots.

## Audio

gbarecomp has no GAX (Shin'en) support or HLE. GAX runs as ordinary recompiled code on the emulated FIFO/DMA hardware.
Validate against a trusted emulator by ear and with `GBARECOMP_AUDIO_FIFO_TRACE`.

## Performance suite (global rule)

`GBARECOMP_FRAME_PHASE=<csv>` gives one row per presented frame. `GBARECOMP_PRESENT_CADENCE=1` gives present timing.
Neither reports 1%/0.1% lows, hitch counts (>25/33/50/100 ms) or stutter (>4 ms frame-to-frame), so add
`tools/frametime_report.py` on top of the CSV. Scenarios: cold start, warm start, menus, world map, level traversal, scene loads,
each run at least twice, compared against a baseline build.

## Open checks

- 10,324 discovered entries; 10,318 are inside the code region (fine-grained jump-table and branch entries), and 6 are near 0x081C0000.
- Save type (EEPROM / SRAM / flash).
- Whether the decomp's labels can seed `[[extra_func]]` names.
