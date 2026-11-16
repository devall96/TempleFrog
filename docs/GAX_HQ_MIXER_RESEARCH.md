# TempleFrog native ("enhanced") mixer for the GAX-attributed sound engine: research

Status: research only. No source was changed, nothing was built or run. Every ROM fact below comes from static
reading of `roms/frog_us.gba` (SHA-1 `7b4c27009198df18555e63fb5dcad223eaf09815`) through the recompiler's own disassembly comments in
`generated/recompiled_00*.cpp`, plus a small ARM decode of routines the recompiler never emitted
(`0x080331E0`, `0x080336AC`). Facts marked **(dump)** are predictions to confirm at run time (section e).

Sources:
- JRickey/frog-adv-temple-decomp: `docs/gax-library.md`, `docs/subsystems.md` "Audio / sound", `docs/memory-map.md`,
  `docs/unknowns.md`, `include/sound.h`, `lib/gax/README.md`, `lib/gax/reference/*.c`. The repo is MIT-licensed
  (`LICENSE`), with a clean-room policy ("do not consult leaked Konami source"). MIT would allow reuse with the
  notice kept, but nothing below copies its code. Only layouts and addresses are used, and most of them were
  re-derived from the ROM.
- gbarecomp at `gbarecomp/`, branch `templefrog/presentation`, commit `edf63c8`.
- Public GAX notes: loveemu's GAX research gist (https://gist.github.com/loveemu/9b3063ffd9a76cb18e379324e43f3251)
  and GAX game list (https://gist.github.com/loveemu/23baff16464b35c5b4c9bb2fdb7d450f).

---

## (a) The engine in this ROM

### Identity and version

- The decomp labels `0x0802D5EC–0x080338A8` (139 functions) as GAX by Shin'en (`docs/gax-library.md`), based on
  byte identity with other GAX titles.
- **The ROM has no GAX version string.** GAX 2/3 builds carry a "GAX Sound Engine vX.XX" banner; a search finds
  none. The only `Gax` bytes, at file offset `0x38B4BF`, sit inside 8-bit sample data. They are not a marker.
- **It does not behave like the public GAX 2/3 descriptions:**
  - It mixes at an arbitrary rate (8192 Hz). GAX 3 picks from a fixed list: 5735, 9079, 10513 Hz and so on.
  - It reads samples as **signed** 8-bit (`ldrsb`). loveemu notes GAX 2+ samples are unsigned.
  - It drives the 4 PSG channels as well as the PCM voices.
  - Mixing is paced by a **Timer1 IRQ every 128 samples**, not by VBlank.
- Conclusion: treat it as an early GAX (v1-era, 2001) or a licensee variant. Public GAX format knowledge
  (song/instrument headers, FX slot lists, rate tables) should **not** be assumed. The design below depends only on
  the mixer ABI read from this ROM, which is small and fully understood.
- The game is not in loveemu's GAX game list.

### Configuration (IWRAM header at `0x03000000`)

The game-side init `0x08020388` fills a parameter block and calls the engine. Literals are in its pool.

| Addr | Value (static) | Meaning | Evidence |
|---|---|---|---|
| `0x03000000` u16 | `0x2000` = **8192 Hz** | requested mix rate | `0x0802038A..0x08020392` |
| `0x03000002` u8 | `8` | number of PCM voices ("count") | `0x08020396..98` |
| `0x03000003` u8 | `0` | mixer flags: bit0 = nearest (no interp), bit1 = mono, bit2 = alt FX, bit3 = run from ROM | `0x08020394`; decoded at `0x0802FB16..0x0802FBD2` |
| `0x03000004` u16 | not written here **(dump)** | FX/echo delay in ms (0 = no FX buffer) | used at `0x0802FE2E..0x0802FE92`, `0x08032C70` |
| `0x03000008` u32 | `0x083181C0` | sound data bank (→ `S+0x110`) | `0x0802039A`, `0x0802D64A` |
| `0x03000010..1F` | sizes: state, code, accum, FX | written by sizing fn `0x08032C18` | |
| `0x03000020..2F` | map: `[0]` state S, `[4]` code `0x03000030`, `[8]` accum, `[0xC]` FX | `0x08032E04` | |

With flags 0, the init `0x0802FAB8` copies three ARM routines into IWRAM. They match the three `[[code_copy]]`
entries already in `game.toml`:

| IWRAM | ROM source | Role |
|---|---|---|
| `0x03000030` (0x1E0 B) | `0x08033000` | **voice mixer**: stereo, linear interpolation (mode 0) |
| `0x03000210` (0x114 B) | `0x08032E54` | **packer**: s16 stereo accumulator → s8 FIFO A (L) / FIFO B (R), clear accumulator |
| `0x03000324` (0xA4 B) | `0x080336AC` | **FX/echo** bus (stereo) |

The other variants exist but are not selected with flags 0: `0x080331E0` (nearest), `0x0803336C`/`0x08033520`
(mono), `0x08032F68` (mono pack), `0x08033750`/`0x08033800` (alt FX).

The state block S sits right after the code: S = `0x03000030 + 0x398` = **`0x030003C8`** **(dump)**, and the
pointer slot **`0x030065E0` = S**.

- The writer of `0x030065E0` is the engine init `0x0802D5EC` (`str` at `0x0802D624`). It is not `sub_0802D558`, as
  `docs/memory-map.md` says; `0x0802D558` is only the CpuFastSet wrapper.

### State block S (base = `*(u32*)0x030065E0`)

The layout is built by `0x0802D5EC` for count = 8. Offsets agree with `include/sound.h`'s `SoundSystem`; the meanings
below are re-derived from the code.

| Offset | Type | Meaning |
|---|---|---|
| `+0x00` | u8 | count (8 PCM voices) |
| `+0x02` | u16 | actual mix rate in Hz (8192); the divisor in `Sound_CalcNotePeriod` |
| `+0x0C`, `+0x0E` | u16 | master volumes 1..256 (game sets `0xCD`+1 and `0xF5`+1 via `0x0802E32C`/`0x0802E344`) |
| `+0x10..+0xBF` | | **PSG** (CGB) channel state for the 4 hardware channels (ids 0..3) |
| `+0xC0` | ptr | **mix-voice array**, count × 28 B (predicted S+0x214 = `0x030005DC`) |
| `+0xC4` | ptr | **active-voice pointer table**, count × u32; NULL = idle (S+0x1F4 = `0x030005BC`) |
| `+0xC8` | ptr | per-voice "slot" (envelope/pan/pitch accumulators), count × 64 B (S+0x314) |
| `+0xCC` | ptr | slot pointer table (S+0x2F4) |
| `+0xD0` | 4 × fn | main mixer, FX-voice mixer, pack, FX routine (IWRAM addresses) |
| `+0xE0` | ptr | s16 stereo accumulator, 128 frames = 0x200 B (predicted `0x03000AF8`) |
| `+0xE4` / `+0xE8` | ptr | FIFO A / B rings, 3 × 128 B each (`0x03000CF8` / `0x03000E78`) |
| `+0xEC` | ptr | end of rings |
| `+0xF0` | u8 | ring chunk index 0..2 |
| `+0xF1` | u8 | "started" (Timer1 armed) |
| `+0xF2` | u16 | Timer1 reload = −(2 × 2048) = `0xF000` |
| `+0xF4` | 28 B | **FX block** (below) |
| `+0x10E` | u8 | FX enabled (the FX routine runs only when this is non-zero) |
| `+0x10F` | u8 | copy of the flags byte |
| `+0x110` | ptr | sound bank |
| `+0x114`, `+0x118`, `+0x120` | | sequencer, request block and handle tables (sequencer side; not needed by the mixer) |

Channel ids 0..3 are PSG and ids ≥ 4 are PCM voices, so slot (id − 4) is used for PCM (`0x0802F9F0`, `0x0802F99C`).

### Mix voice (28 bytes; the mixer receives it in `r0`)

Decoded from `0x03000030` (ROM `0x08033000`):

| Off | Type | Meaning |
|---|---|---|
| `+0x00` | ptr | sample header (`+8` u16 = native rate in Hz, `+0xA` u8 = root note; used by `Sound_CalcNotePeriod`, `0x0802FA60`) |
| `+0x04` | u8* | **current position** (byte address of an s8 sample) |
| `+0x08` | u8* | **end** pointer. If end < cur (signed compare), the voice plays **backwards** |
| `+0x0C` | u8* | **loop start**; 0 = one-shot |
| `+0x10` | u32 | **loop length**. On wrap, end ← loopstart ± length |
| `+0x14` | u16 | **step**, 4.12 fixed point, in source samples per 8192-Hz output sample |
| `+0x16` | u16 | **fractional position** (low 12 bits) |
| `+0x18` | u8 | **left volume** (0 = silent, else gain (v+1)/256) |
| `+0x19` | u8 | **right volume** |
| `+0x1A` | u8 | invert right (surround). Set for pan ≥ 0x80 (`SoundPan_UpdateMixEntry`) |
| `+0x1B` | u8 | FX send. Non-zero means the voice is mixed into the FX send bus, not the main bus |

### Exact per-sample behaviour of the guest mixer (mode 0)

For each of 128 output samples:

1. `s0 = (s8)*cur`, `s1 = (s8)*(cur+1)`. If `cur+1 >= end`, then `s1 = loopstart ? *loopstart : s0`.
2. Interpolate: `x = s0 + (((s1 − s0) × frac) >> 12)` (12-bit linear interpolation).
3. Accumulate: `L += (x × (volL+1)) >> 8` and `R += (x × ±(volR+1)) >> 8`, with arithmetic shifts. A volume of 0
   gives gain 0.
4. Advance: `frac += step`. If `frac ≥ 0x1000`, then `cur += frac >> 12` and `frac &= 0xFFF`.
5. If `cur ≥ end` (forward direction):
   - With no loop: store, then return 1 (the voice ended). The caller NULLs `S+0xC4[i]`.
   - Otherwise: `cur = cur − end + loopstart`, `end = loopstart + looplen` (written back to `+0x08`), and if
     still `cur ≥ end`, `cur = loopstart`.
   - Reverse playback mirrors this (`0x03000144..0x030001F8`): `end = loopstart − looplen`.

On exit the mixer writes `+0x04` and `+0x16` back. Nothing else in the voice is changed by mixing.

The packer `0x03000210` then, for 128 frames, clamps L and R to ±127 with no shift and writes s8 to FIFO A (L) and
FIFO B (R) at ring offset `idx × 128`. It also zeroes the accumulator.

The full-scale model is therefore: a voice at volume 255 contributes the s8 sample at unit gain; voices sum and
**hard-clip** at ±127.

### Hardware setup (`0x0802FED2..0x0802FF2A`, mix entry `0x0802FFD8`)

- **Timer0** sets the sample rate: reload −2048 (`0xF800`), prescaler 1, which gives 16777216 / 2048 = **8192 Hz**.
  The period is masked to a multiple of 64 cycles (`& 0xFFC0`, `0x0802FD2C`) so that Timer1 divides it exactly.
- **Timer1** sets the chunk rate: control `0xC1` (enable, IRQ, F/64), reload `0xF000`, armed on the first mix
  (`0x08030012..0x08030020`). Its period is 4096 × 64 = 262144 cycles = **128 samples**, so **64 chunks/s**. A frame
  is 280896 cycles, so there are about 1.07 chunks per frame, **not frame-locked**.
- **DMA1** copies `S+0xE4` (ring A) to FIFO A `0x040000A0`; **DMA2** copies `S+0xE8` to FIFO B `0x040000A4`. Both
  use CNT_H `0xB660` (enable, FIFO timing, repeat, 32-bit, fixed destination).
  - Every third chunk (when `S+0xF0` becomes 1) the mixer stops and restarts both DMAs at the ring start
    (`0x0803003E..0x0803005C`).
  - So the chunk mixed into slot k is played during the next chunk period: **exactly one chunk (15.6 ms) of
    latency**.
- **IRQ path:**
  - `REG_IE = 0x2011` (VBlank, Timer1, GamePak).
  - IRQ table `0x08035D9C[4]` (Timer1) → `0x0800090C` → veneer `0x08035D84` → ARM `0x08000240`.
  - If ROM word `0x082F9010` (= 1) is set, `0x08000240` masks VBlank in IE, re-enables IRQs and calls
    **`0x0802FFD8`** (the chunk mixer).
- **`0x0802FFD8`, the chunk mixer, in order:**
  1. Arms Timer1 on first use, then bumps the ring index (with the DMA restart above).
  2. For each voice i < count with `S+0xC4[i]` non-NULL: calls `fn[S+0xD0]` (or `fn[S+0xD4]` into the FX send bus
     at `S+0xF4` if `+0x1B` is set) with the accumulator. A return of 1 means the voice is removed.
  3. If `S+0x10E` is set, calls the FX routine with `(S+0xF4, accumulator)`.
  4. Calls the packer with `(accum, ringA + idx×128, ringB + idx×128)`.
- SOUNDBIAS (`0x04000088`) is never written by the engine, so the BIOS default `0x200` stands **(dump)**, giving a
  gbarecomp grid rate of 32768 Hz. SOUNDCNT_H routing is written through `0x0802E360` (high byte of `0x04000082`);
  the expected value is A→L only and B→R only **(dump)**.

### FX / echo (`0x080336AC`, FX block at `S+0xF4`, set up in `0x0802FE2E..0x0802FEAC`)

**This game never uses it.** The game-side setup `0x08020388` writes only the rate (`+0`), voice count (`+2`), flags
(`+3`) and bank (`+8`) of the parameter block; nothing in the code area addresses `0x03000004`, so the FX delay stays
0 and the sizing call reports an FX buffer of 0 bytes (`0x0300001C` = 0 in every dump). `S+0x10E` stayed 0 across
414 s of title and attract demos.

| Off | Meaning |
|---|---|
| `+0x00` | send accumulator (s16 stereo, 128 frames). Voices with `+0x1B` set mix here |
| `+0x04` | delay ring start |
| `+0x08` | write pointer |
| `+0x0C` | read pointer (one chunk ahead of write when delay > 1 chunk) |
| `+0x10` | ring end |
| `+0x14` u16 | feedback, default `0x7332` (≈ 0.45) |
| `+0x16` u16 | dry gain, `0xFFFF` (≈ 1.0) |
| `+0x18` u16 | wet gain, `0xCCCC` (≈ 0.8) |
| `+0x1A` u8 | 1 = active |

The delay is `params[4] (ms) × rate / 128000` chunks, clamped to 1..255, so the ring is that many 128-frame chunks.

Per halfword (L and R independent, no ping-pong):

```
out    = (wet·ring[read] + dry·send) >> 16
ring[write] = send + (fb·ring[write] >> 16)
main  += out
send   = 0
```

The read and write pointers wrap at the end of the ring.

### Sample format, instruments, sequencer

- Samples are raw **signed 8-bit PCM** addressed by absolute pointers (ROM, or possibly EWRAM **(dump)**).
  - Loops are defined by `loopstart` and `looplen` in bytes. Backward playback is supported.
  - There is no compression in the mixer path.
- Pitch: `step = rate_hz(hdr+8) × ratio(note − root, frac) / S+0x02`. The ratio comes from 4.12 semitone LUTs
  (`Sound_CalcNotePeriod` in `lib/gax/reference/sound_period.c`; LUTs in the decomp's `src/data/sound_pitch.c`).
  - So `step × 8192 / 4096` is the sample's playback rate in Hz.
  - Samples pitched above 8192 Hz (step > 0x1000) are decimated without a filter by the guest, which causes aliasing.
- The bank at `0x083181C0` begins with an offset table (`0x20, 0x23D8, …`) used by the sequencer (`S+0x110`,
  `+0x114`, offsets `+0x410`/`+0x414`). **The native mixer never needs to parse it**, because every voice carries its
  own sample pointers.

---

## (b) Engine flow and what determines the output

| When | Code | Writes |
|---|---|---|
| VBlank IRQ (59.73 Hz) | `0x08000790` → `0x0802F4B0` ("SoundMixer_VBlankUpdate" in the decomp). Sequencer opcodes `0x0802D5EC+` / `Sound_OpcodeDispatch`; envelopes A/B/C; pan tick; PSG MMIO writes (`0x04000062..0x04000078`, wave RAM `0x04000090`) | Mix-voice `+0x14` step (via `0x0802FA60`), `+0x18/+0x19/+0x1A` (pan × vol × master, pan law ≈ constant power: centre = `0xB538`/65536 ≈ −3 dB, `0x0802F3EE..`). Note-on writes `+0x00..+0x10` and sets `S+0xC4[i]`. PSG registers directly |
| Timer1 IRQ (64 Hz) | `0x0802FFD8` → `0x03000030` × active voices → [`0x03000324` FX] → `0x03000210` | Advances `+0x04`/`+0x16` (and `+0x08` on loop); NULLs ended voices; fills one 128-byte slot of each FIFO ring |
| Timer0 + DMA1/2 (8192 Hz) | hardware | FIFO A/B |

All sound-state mutation from the main thread is bracketed by `0x0802E418` / `0x0802E3F8`. These end in ARM
`0x08032E38` / `0x08032E1C`, which clear and set **IE bit 4 (Timer1)**. VBlank is masked while a chunk mixes. **A
chunk therefore always sees a consistent voice set.**

**What fully determines one chunk of PCM output** (all read at entry to `0x0802FFD8`):

- For each i < `S+0x00` with `p = S+0xC4[i]` non-NULL: the 28-byte voice at p, i.e. cur, end, loopstart, looplen,
  step, frac, volL, volR, inv, fx.
- The sample bytes those pointers address.
- `S+0x10E`, the FX block `S+0xF4..+0x10F` and the delay-ring contents. The ring is the guest's own history, so a
  native renderer keeps its own float ring.
- Constants: `S+0x02` rate, chunk = 128 frames.

Volumes and step are constant within a chunk. They change only at the chunk boundary at which the guest picks up
the VBlank tick's writes. That is a 64-Hz staircase, which is the source of "zipper" steps on fades.

**PSG:** the 4 hardware channels are written straight to MMIO at VBlank. gbarecomp already synthesises them; the
native mixer leaves them on the emulated path.

---

## (c) How MP2K "enhanced audio" works in gbarecomp

Paths are relative to `gbarecomp/`. Line numbers are at `edf63c8`.

- **Detection and arming:**
  - `mp2k_detect()` (`src/gba/gba_m4a.cpp:61-98`) scans the ROM for the SoundMain literal pool, with a CRC
    fallback.
  - `GbaBus::set_rom` → `audio_.configure_shadow(...)` (`src/gba/gba_bus.h:142-154`).
  - The struct map is in `src/gba/gba_m4a.h:37-42`: SoundInfo pointer `0x03007FF0`, magic `'Smsh'+1`, channels at
    `+0x50`, stride `0x40`.
- **Reading guest state:** `MemView` (`src/gba/mp2k_shadow.h:32-41`, `mp2k_shadow.cpp:36-63`) holds raw const
  pointers to ROM, EWRAM and IWRAM. There is no bus access, no waitstates and no side effects.
  - `Mp2kShadow::frame_hook(const MemView&, uint64_t audio_cursor, uint32_t key)` (`mp2k_shadow.cpp:75-210`)
    re-reads every channel on each driver tick.
  - It mirrors the driver's envelope step to get this tick's gain and a one-tick-ahead gain.
- **Host mix:** `Mp2kShadow::render` (`mp2k_shadow.cpp:212-277`).
  - Float, at a fixed 65536 Hz.
  - Linear interpolation on a `double` position; `fmod` loop wrap; DPCM decode.
  - Per-sample gain interpolated between ticks, plus a ≈0.3 ms one-pole glide; ending voices fade out.
  - A reverb tap; output scaled ×0.25 (full-scale DAC).
  - It also renders a driver-domain "check copy" for the verifier.
- **Substitution point:** `GbaAudio::mix_slot(uint32_t slot)` (`src/gba/gba_audio.cpp:1056-1119`).
  - When `shadow_enabled_ && sample_rate()==65536`, it calls `shadow_.render(...)` with the live FIFO bytes as a
    reference.
  - If `shadow_.live()`, it mixes PSG only (`mix_one_sample(slot, include_direct=false)`).
  - PSG then goes through an 8 kHz Butterworth (`psg_low_pass`).
  - It adds the shadow's two streams **as FIFO A/B**, routed through SOUNDCNT_H volume and L/R enables
    (`:1094-1105`), and clamps to S16.
- **Output pipeline:**
  - `GbaAudio::tick` → `run_sample_event` every 1024 cycles → `mix_slot` per slot → stereo S16 ring tagged with the
    SOUNDBIAS rate.
  - The rate is `16777216/(0x200>>res)`, i.e. 32768/65536/131072/262144 (`gba_audio.h:39-46,89`;
    `update_soundbias` `gba_audio.cpp:613`).
  - Once per frame `drain_host_audio` (`src/runtime/runtime.cpp:4415-4442`) → `drain_stereo_sample_block` →
    `gba_mod_audio_mix_stereo_at_rate` → `HostWindow::push_audio_stereo_block`.
  - SDL S16 stereo through the band-limited DRC bridge `recomp_audio_drc.h` (`host_window.cpp:2209-2264`,
    `:2397-2428`) resamples to the device rate (for example 48 kHz).
- **Sync:**
  - It does not poll at VBlank.
  - A dispatch observer (`RuntimeDispatchObserver`, `src/armv4t/runtime_arm.h:76-83`; fired in `dispatch_resolve`
    `runtime_arm.cpp:1015-1020`; installed `runtime.cpp:2045-2053` for `shadow_hook_pcs()`) calls
    `GbaAudio::shadow_driver_tick` (`gba_audio.cpp:1003-1009`) when SoundMainRAM is entered.
  - `shadow_cursor_` counts grid samples between ticks.
  - The verifier (`ShadowVerifier::judge`, `audio_shadow.cpp:121-179`) correlates envelopes over about 1 s and
    reverts on mismatch, logging "MP2K shadow DEGRADED".
- **Toggle and persistence:**
  - Menu key `audio.enhanced` (`runtime.cpp:265`), shown only if `shadow_available()` (`:3708-3717`).
  - Get at `:320-321`. Set at `:376-381`: `runtime_mmio_catch_up(); set_shadow_enabled(v); runtime_resync_horizon();
    window->save_audio_enhanced(v)`.
  - `config.ini [Audio] enhanced=0|1` (`host_window.cpp:1245-1248`, `:3447-3471`).
  - Precedence: env `GBARECOMP_AUDIO_SHADOW` > saved choice > `game.toml [audio] shadow` (`gba_audio.cpp:967-974`).
  - `set_shadow_enabled` (`:981-991`) rebuilds a fresh shadow, which has to prove itself again.
- **Determinism:**
  - Shadow state is not serialised (`GbaAudio::serialize` `gba_audio.cpp:47`).
  - Multiplayer refuses it: "audio shadow is not qualified for multiplayer" (`src/gba/gba_simulation_state.cpp:20`).
  - Presentation may differ only downstream of guest state (`PRINCIPLES.md:173-176`).
  - `PRINCIPLES.md:170-172` and `HLE.md:16-19` call the audio shadow a *historical interface*, not the pattern for
    new HLE.
  - `ENHANCEMENTS.md:3-5`: elective presentation changes are opt-in and off by default; "shared mechanisms belong in
    the engine; scene policy belongs in each game."
- **Hook points a game plugin can use without touching guest state:**
  - **Trusted function-entry hook** (`src/runtime/mod_function_hooks.h`; `docs/TOML_SCHEMA.md`
    `[[mod_function_hook]]`; `docs/MOD_PACKAGES.md` "Trusted function-entry hooks"):
    `gba_mod_register_function_entry_plugin(id, addr, thumb, cb)`.
    - A callback that **returns 0 declines**, and its CPU writes are discarded, so it acts as a pure observer at
      the exact guest instant of entry.
    - It needs a `[[mod_function_hook]]` TOML entry. Codegen emits `if (gba_mod_function_entry(addr, thumb, &g_cpu))
      return;`.
    - `0x0802FFD8` is a static root (`generated/recompiled_000.cpp:389211`, `gf_tfunc_0802FFD8`).
  - `g_runtime_fn_entry_hook` (`runtime_arm.h:398-407`) is a single global, debug-only, and runs on every function.
    Not suitable.
  - `gba_mod_audio_stream_register_s16_mono` and `gba_mod_audio_stream_set_native_gain_percent` (`src/gba/mod_audio.h`,
    `docs/MOD_AUDIO.md`) are **mono only**, and gain 0 mutes **all** native PCM including PSG. Not sufficient.
  - **There is no public stereo "replace direct sound" seam and no public read-only guest-memory accessor for
    plugins.** Both need a small engine addition (section d).
- **A/B tooling that already exists:**
  - `GBARECOMP_AUDIO_FIFO_TRACE=1` (`gba_audio.cpp:29-31,593-606`, TCP `audio_trace`).
  - Always-on capture ring of 2^18 samples with raw `direct_a`/`direct_b` (`gba_audio.h:94-121`, TCP `audio_cap`).
  - `GBARECOMP_AUDIO_PROBE=1`.
  - `oracle/diff_m4a.py`.
  - There is **no WAV dump**.

---

## (d) Design: the TempleFrog native mixer

The feature is a presentation-only enhancement, not HLE:

- The guest engine, including its own IWRAM mixer, keeps running unchanged, so FIFO bytes, DMA, timers, IRQ timing,
  savestates and netplay digests do not change.
- The host only *reads* guest memory at one guest instant per chunk and replaces what the player hears from FIFO
  A/B.
- It is off by default and per player, following `ENHANCEMENTS.md`.

### 1. Snapshot (read-only, exact)

- Hook: `[[mod_function_hook]] addr = 0x0802FFD8, mode = "thumb"` in `game.toml`. The callback always returns 0.
- At entry, before the guest mixes, read through raw const pointers (no `bus_read`, no `runtime_tick`):
  - `S = *(u32*)0x030065E0`. Validate that it lies in IWRAM, `S+0x00 ∈ 1..16`, and `S+0x02 ∈ 2000..22050`.
  - `count`, `rate = S+0x02`, `flags = S+0x10F`, `fx_on = S+0x10E`, the FX block `S+0xF4..+0x10F`.
  - For i < count: `p = S+0xC4[i]`; if non-NULL, copy the 28-byte voice and remember `(i, p)`.
- Stamp the snapshot with the audio grid cursor (grid samples elapsed) and push it into a fixed-size SPSC ring of
  about 8 chunks. Everything runs on the emulation thread; there is no locking.
- The first entry, with `S+0xF1 == 0`, also starts Timer1. Treat it as stream start.
- Why this is exact: the guest renders the chunk entirely from this snapshot, and VBlank and sound mutators are
  masked while it does. The host sees exactly the inputs the guest uses, at the same guest instant.

### 2. Render (float, any output rate)

Work per chunk. The host renders `N = 128 × grid_rate / rate` frames: **512 at the 32768-Hz grid**, 1024 at 65536,
or **750 at 48000**. All are exact integers for the 8192-Hz mix rate.

**Position.** Compute it analytically from the snapshot, so it cannot drift:
`pos(j) = cur + (frac + j × step × rate / grid) / 4096`. Use 64-bit fixed point or `double`, with direction set by
`end < cur`. Apply the same loop rule as the guest: on crossing `end`, `pos += loopstart − end` and
`end = loopstart ± looplen`; with no loop, the voice ends.

At the next snapshot the guest's `cur/frac` should equal the host's predicted end state. Keep an assert counter.

**Interpolation:**
- Default 4-point cubic (Catmull-Rom/Hermite) on s8/128 floats.
- Neighbour taps follow the loop: indices past `end` wrap to `loopstart`; with no loop, they hold the last sample,
  as the guest does.
- Linear is an option.
- For `step × rate / grid > 1` (rare: a sample rate above the grid rate), fall back to linear plus a one-pole
  pre-filter, or a 2× box. It never happens at 32768 for normal samples.

**Gain:**
- `gL = (volL ? volL+1 : 0)/256` and `gR = ±(volR ? volR+1 : 0)/256`, where the sign comes from `+0x1A`.
- Optional "smooth volume": ramp linearly from the previous chunk's gains to the new ones over the first 64 grid
  samples (2 ms). This removes the 64-Hz staircase.
- A voice whose pointer or cur jumps (a new note) starts with no ramp, to keep attacks sharp.
- Per-voice continuity is keyed by voice index plus the predicted cur.

**Buses:**
- Voices without `+0x1B` go to main L/R; voices with it go to a float FX send.
- If `fx_on`, run the echo exactly as decoded in section (a), in float. The delay in frames is
  `(ring_end − ring_start)/4 × grid/rate`, with `fb = (v+1)/65536`, `dry` and `wet` the same.
- On enable, seed the float ring from the guest ring once (it is in IWRAM/EWRAM at `S+0xF4+4..+0x10`), or start
  silent.

**Level:**
- Sum to `mixL/R` in "s8 units" (guest full scale = 127), then output `fifo_a = mixL/128`, `fifo_b = mixR/128` in
  the bus float domain that `mix_slot` already uses for MP2K (×512 → SOUNDCNT_H volume ×2/×4 → ×48).
- The guest hard-clips at ±127. Provide a soft-clip (tanh-knee above 0.9) as the default and a "faithful hard clip"
  option. Measure how often the guest clips (FIFO bytes at ±127) during validation.

**Latency:**
- The guest plays chunk k one chunk after mixing it. Queue rendered chunks and start each one a chunk after its
  snapshot cursor. This gives identical timing to the original for A/B, and the one-chunk prebuffer absorbs the
  64-Hz versus 59.73-Hz beat and IRQ servicing jitter.
- If the queue runs dry, for example after a late IRQ, emit 0 and count an underrun. If it grows past 3 chunks, drop
  the oldest and count it.

### 3. Output rate (on the 48 kHz request)

- **Recommended:** render at the gbarecomp grid rate (32768 Hz here, since SOUNDBIAS is the default) in float and
  hand it to `mix_slot`. The host bridge's band-limited resampler (`recomp_audio_drc.h`) converts to the 48-kHz
  device.
  - Bandwidth rises from the original 4.1 kHz Nyquist (plus zero-order-hold images) to 16.4 kHz, more than the
    content needs.
  - PSG mixing, SOUNDCNT_H routing and the master volume path all stay unchanged.
- **Alternative:** a native 48-kHz stereo host stream that bypasses the grid. This needs a new stereo pull-stream
  API in `mod_audio` and has a second clock domain to keep in sync. It is only worth doing if 32768 is shown to be
  audibly limiting.
- The renderer takes the output rate as a parameter either way, and N = 750 at 48 kHz is exact.

### 4. Where it plugs in (code structure)

**Engine side (gbarecomp; shared mechanism; small):**

1. `src/gba/direct_sound_source.h` (new). It defines a presentation-only interface plus a registration call:

   ```cpp
   struct GbaDirectSoundSource {
     // Emulation thread, once per grid sample, only while enabled.
     // Returns true and FIFO-domain floats (s8 full scale = 1.0) to replace FIFO A/B;
     // false → canon mix.
     bool (*render)(void* ctx, uint64_t grid_cursor, uint32_t grid_rate, float* fifo_a, float* fifo_b);
     void* ctx;
   };
   int gba_audio_set_direct_sound_source(const GbaDirectSoundSource*);   // activation-time only
   ```

   The plugin's hook needs the cursor, through an accessor such as `uint64_t gba_audio_grid_cursor(void);`. This
   must be a read-only snapshot.
2. In `GbaAudio::mix_slot` (`gba_audio.cpp:1056`), generalise the MP2K branch:
   - Try the MP2K shadow, else the registered source.
   - On substitution, reuse the existing path: PSG-only canon + `psg_low_pass` + SOUNDCNT_H routing of the two FIFO
     floats.
   - Unlike the MP2K branch, do **not** gate it on `sample_rate()==65536`; pass `sample_rate()` in.
   - Record what was substituted in the capture ring (`direct_a`/`direct_b`) so A/B tooling can see both.
3. A read-only guest memory view for plugins: export the existing `MemView` (`mp2k_shadow.h:32-41`) through
   `const MemView* gba_mod_guest_memview(void)`. It holds const pointers only.
4. Make the "Enhanced audio" menu item (`runtime.cpp:265`, `:3708-3717`, `:376-381`) appear when *either*
   `shadow_available()` or a direct-sound source is registered. Persist it under the same
   `config.ini [Audio] enhanced`.
   - The toggle must not rebuild guest state: just flip `enabled`.
   - On enable, substitution begins at the next snapshot, with a 5 ms fade-in. On disable, return to canon at once.
5. Multiplayer: mark the source presentation-only and do not serialise it. Simulation stays identical because no
   guest write is possible. Keep the "shadow not qualified" error MP2K-specific.

**Game side (TempleFrog repo; GAX policy):**

- `src/gax_mixer.h/.cpp`: a pure renderer with no gbarecomp includes. It takes snapshot structs in and produces
  float frames, so it can be unit-tested. It has two modes:
  - `Faithful` (integer-exact replica of section (a)'s math at 8192 Hz with s8 clamp), used for validation.
  - `Enhanced` (float, cubic, grid rate, optional smoothing and soft clip).
- `src/gax_plugin.cpp`: the function-entry callback (snapshot + push, returns 0) and the source `render` (pops and
  renders chunks).
  - Registration follows the shape of `src/hqx_plugin.cpp`: `GBA_MOD_CONSTRUCTOR` →
    `gba_mod_register_function_entry_plugin("templefrog.audio.gax", 0x0802FFD8, 1, cb)` +
    `gba_mod_register_activation_plugin` + reset callback.
  - Reset or savestate load clears the queue and the continuity state.
- `game.toml`: add `[[mod_function_hook]] addr = 0x0802FFD8 mode = "thumb" note = "GAX chunk mixer; observer only, always declines"`.
  `[audio] shadow = false` stays.
- Package `mods/preloaded/packages/templefrog.audio.gax/` with options `interp` (cubic/linear), `smooth_volume`,
  `clip` (soft/hard) and `surround` (keep/flatten).
- `tests/gax_mixer_test.cpp`: golden chunks.
- `tools/gax_ab.py`: offline comparison.

### 5. Validation

1. **Bit-exact state capture (gate).** Run the renderer in `Faithful` mode on every snapshot and compare its 128 s8
   L/R bytes with what the guest packer actually wrote.
   - Read the ring slot `S+0xE4/E8 + idx×128` at the *next* hook, or hook the packer `0x03000210` as a second
     observer.
   - **The target is 0 differing bytes over long runs** (title, menus, world map, levels, all SFX).
   - Any mismatch means a misread field (FX, reverse, loop edge) and must be fixed before listening tests.
2. **Prediction check.** The host's end-of-chunk `(cur, frac, end)` must equal the next snapshot's for continuing
   voices. Count mismatches.
3. **A/B capture.** Add a `GBARECOMP_AUDIO_WAV=<prefix>` dump of three streams:
   - the post-`mix_slot` S16;
   - the canon FIFO-only mix (A/B bytes held at 8192 Hz);
   - the native FIFO-domain floats.
   Run the same input recording twice, enhanced on and off.
4. **Spectral and RMS.** Band-limit the native output to 4096 Hz and decimate to 8192.
   - The RMS difference from the canon should then sit near the 8-bit quantisation floor (≈ −48 dBFS) plus the
     linear-versus-cubic difference.
   - Report RMS difference in dB per scenario, and the energy above 4 kHz in the canon output (images) versus the
     native output.
5. **Note timing.** Cross-correlate canon and native output per scene. The lag must be 0 ± 1 grid sample (both start
   from the same chunk boundary). Per voice, compare note-on chunk indices; they must be equal by construction.
6. **Queue health.** Underruns and drops must be 0 in all scenarios. Log the queue depth histogram.
7. **Performance (global rule).** Frame-time suite with per-present CSV (`GBARECOMP_FRAME_PHASE`,
   `GBARECOMP_PRESENT_CADENCE`, `tools/frametime_report.py`).
   - Report 1%/0.1% lows, p50/p99/p99.9/max, hitches over 25/33/50/100 ms, and stutter over 4 ms.
   - Cover cold/warm start, menus, world map, level traversal and scene loads, each run twice, enhanced on versus off
     versus the baseline build.
   - Expected cost is tiny (8 voices × 32768 × cubic ≈ 1 M MAC/s), but the hook is on an IRQ path, so check it.
8. **Listening.** Compare by ear with a trusted emulator (mGBA) as `HOST_PLAN.md:55-56` already requires.

### 6. Risks

1. **The engine is not textbook GAX.** It has no version string, a Timer1 chunk IRQ, PSG channels, signed PCM and an
   arbitrary rate. Public GAX documentation cannot fill gaps; every field must come from this ROM. That has been
   done for the mixer path. Mitigation: the bit-exact gate (validation 1).
2. **FX/echo path.** `0x03000324` was never observed running (`game.toml` note). If some scene enables `S+0x10E` or
   sets `+0x1B`, an echo bus that is wrong or missing changes the sound. Mitigations:
   - Implement it from the decoded routine.
   - Gate on `fx_on`.
   - If validation 1 fails only with FX active, fall back to canon for those chunks.
3. **Sync and clock domains.**
   - Chunks arrive at 64 Hz and frames at 59.73 Hz, and gbarecomp services IRQs at block boundaries, so the hook
     instant jitters.
   - Savestate load and reset mid-chunk, fast-forward and the toggle mid-song must not cause bursts or gaps.
   - Mitigations: cursor-stamped queue with a one-chunk prebuffer (the same latency as hardware), clear on load, and
     underrun/drop counters.

Further risks:
- **Re-init with a different rate or flags.** The game could call the init again, for example from the options or
  sound test. Re-read `S+0x02` and `S+0x10F` on every chunk and handle the mono/nearest variants, or fall back to
  canon when `flags != 0`.
- **Loudness and clipping.** The float mix will not clip where the guest did. The soft-clip default keeps loudness
  but can sound different in dense moments. Keep a hard-clip option and measure guest clip rates.
- **Surround inversion** (`+0x1A`): faithful reproduction can sound hollow on headphones. Offer a "flatten" option,
  off by default.
- **Sample pointers outside ROM/IWRAM/EWRAM**, or reads past the end of the ROM (backward or looped edge taps):
  bounds-check through `MemView`, return 0 and count it.
- **Policy fit:** the engine docs discourage *new* runtime HLE toggles. This feature is an elective presentation
  enhancement over unchanged guest execution, which `ENHANCEMENTS.md` allows. State that explicitly in the PR so it
  is not mistaken for a second audio shadow.
- **Upstream overlap:** per the global rule, `git fetch` the gbarecomp upstream and check its open PRs for audio
  source or GAX work before implementing. This research did not do that.

---

## (e) Open questions for runtime inspection

Use gbarecomp's TCP debug server, IWRAM dumps (`GBARECOMP_IWRAM_DUMP`) or mGBA.

| # | What | Address / how | Expected |
|---|---|---|---|
| 1 | State pointer | `0x030065E0` | `0x030003C8` |
| 2 | Params, sizes, map | `0x03000000..0x0300002F` | `2000 08 00 ????, …, 0x083181C0`; sizes `{0x730, 0x398, 0x500, fx}`; map `{0x030003C8, 0x03000030, 0x03000AF8, 0x03000FF8}` |
| 3 | FX delay ms and FX on | `u16 0x03000004`; `S+0x10E`; FX block `S+0xF4..S+0x10F` | 0 / 0 if the echo is unused. If non-zero, record the delay, feedback and gains |
| 4 | Rate and flags at run time | `u16 S+0x02`, `u8 S+0x10F`, `u8 S+0x00` | 8192, 0, 8. Re-check after options, sound test and world changes |
| 5 | Voice table and voices | `S+0xC4` → 8 × u32; `S+0xC0` → 8 × 28 B (`0x030005BC`, `0x030005DC`) | table entries point into the voice array |
| 6 | Sample pointers | voice `+0x04/+0x08/+0x0C` across songs and SFX | ROM `0x08xxxxxx`. Any EWRAM means streamed or decoded samples: check alignment and lifetime |
| 7 | Use of reverse play, surround and FX send | count voices with `end < cur`, `+0x1A != 0`, `+0x1B != 0` | probably rare |
| 8 | Step range | max `+0x14` | below `0x1000` for most. Values above mean aliasing in the original |
| 9 | Sample header | `*(voice+0x00)`, 16 bytes | `+8` u16 rate in Hz, `+0xA` u8 root note |
| 10 | Timers and DMA | `0x04000100/102` (TM0), `0x04000104/106` (TM1), `0x040000BC..C6` (DMA1), `0x040000C8..D2` (DMA2) | TM0 `0xF800`/`0x0080`; TM1 `0xF000`/`0x00C1`; DMA src `S+0xE4`/`S+0xE8`, dst FIFO A/B, CNT_H `0xB660` |
| 11 | Routing and bias | `0x04000082` SOUNDCNT_H, `0x04000084` SOUNDCNT_X, `0x04000088` SOUNDBIAS | A→L only, B→R only, 100% (bits 2, 3 set; 8, 13 set); SOUNDBIAS `0x0200`, so a 32768-Hz grid |
| 12 | IRQ cadence | `GBARECOMP_IRQ_LOG`: Timer1 vectors per second, jitter in cycles between `0x0802FFD8` entries | 64/s, 262144 cycles apart |
| 13 | Guest clip rate | fraction of FIFO bytes equal to ±127 per scene | decides the soft/hard clip default |
| 14 | PSG use | writes to `0x04000062..0x0400007C` per scene | confirms which tracks rely on PSG, which stays emulated |
| 15 | Second mixer pointer | `S+0xD4` | equals `S+0xD0` (`0x03000030`) in the stereo config |

Two corrections to the decomp's documentation (they are worth sending upstream):

- **`0x030065E0` is written by `0x0802D5EC`.** The memory map credits `sub_0802D558` (CpuFastSet).
- **`sub_0802F4B0` is the VBlank sequencer/envelope tick, not the sample mixer.**
  - PCM mixing is the Timer1-IRQ chunk routine `0x0802FFD8`, with ARM inner loops in IWRAM.
  - `SoundMixEntry` (`include/sound.h`) is the 28-byte voice: `+0x0C` is the loop start, not `outSample`, and
    `+0x1A` is right-channel inversion, not mono.
