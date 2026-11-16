#pragma once

namespace gba { class ExternalDirectSource; }

// TempleFrog's native high-quality mixer for the game's sound engine (see
// docs/GAX_HQ_MIXER_RESEARCH.md). The game keeps deciding which notes play,
// when and how loud; this mixes the same voices in floating point at the
// output rate with cubic interpolation instead of the GBA's 8192 Hz 8-bit
// FIFO stream. Wired to the in-game "Enhanced audio" toggle.
gba::ExternalDirectSource* templefrog_gax_hq_source();
