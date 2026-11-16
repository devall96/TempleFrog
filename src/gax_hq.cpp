// Native high-quality mixer for Frogger's Adventures' sound engine.
//
// Engine facts (docs/GAX_HQ_MIXER_RESEARCH.md, confirmed by an IWRAM dump):
//   *0x030065E0 -> state block S (0x030003C8). S+0x00 voice count (8),
//   S+0xC4 active-voice pointer table, S+0x10E FX (echo) enabled,
//   S+0xE4/E8 FIFO A/B rings (3 x 128 B), S+0xF0 ring index.
//   Voice (28 B): +04 cur, +08 end (end < cur = reverse), +0C loop start
//   (0 = one-shot), +10 loop length, +14 step (4.12, source samples per
//   8192-Hz output sample), +16 frac (12 bits), +18/+19 volume L/R
//   (gain (v+1)/256, 0 = silent), +1A invert right, +1B FX send.
//   The chunk mixer 0x0802FFD8 runs from the Timer1 IRQ every 128 output
//   samples (64 Hz); FIFO A carries the left mix, FIFO B the right, and a
//   chunk plays one chunk after it is mixed.
//
// At each chunk-mixer entry we snapshot the active voices (read-only) and
// queue them; render() plays the queue one chunk behind, like the hardware,
// resampling every voice straight to the output rate in float with 4-point
// cubic interpolation and no 8-bit quantisation or +-127 clip. A chunk that
// uses the echo bus falls back to the hardware mix.
//
// TEMPLEFROG_GAX_VERIFY=1 re-runs the guest's exact integer mixer on every
// snapshot and compares it with the bytes the game actually wrote to its
// FIFO rings, to prove the snapshot and voice model before trusting the
// float mixer.
#include "gax_hq.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include "gba_audio.h"

namespace {

constexpr uint32_t kStatePtr = 0x030065E0u;
constexpr uint32_t kChunkMixer = 0x0802FFD8u;
constexpr int kChunk = 128;          // guest output samples per chunk
constexpr double kGuestRate = 8192.0;
constexpr int kMaxVoices = 16;

struct Voice {
    uint32_t addr = 0;   // the voice slot's address: identity across chunks
    uint32_t cur = 0, end = 0, loop_start = 0, loop_len = 0;
    uint16_t step = 0, frac = 0;
    uint8_t vol_l = 0, vol_r = 0, invert = 0, fx = 0;
};

struct Chunk {
    std::vector<Voice> voices;
    bool fx_active = false;
};

struct Prediction {
    bool valid = false;
    uint32_t ring_a = 0, ring_b = 0;
    uint8_t idx = 0;
    std::array<int8_t, kChunk> a{}, b{};
};

int8_t s8(const gba::MemView& mem, uint32_t addr) {
    uint8_t v = 0;
    mem.u8(addr, v);
    return static_cast<int8_t>(v);
}

uint16_t rd16(const gba::MemView& mem, uint32_t addr) {
    const uint8_t* p = mem.slice(addr, 2);
    return p ? static_cast<uint16_t>(p[0] | (p[1] << 8)) : 0;
}

// ---- 8-bit de-quantisation ---------------------------------------------------
// The instruments are signed 8-bit PCM: every sample carries up to +-0.5 LSB of
// rounding error, heard as grit that follows the music (worst on soft notes,
// because the engine applies volume after the rounding). A region of sample
// bytes is cleaned once, off the audio path, by projection onto convex sets:
// repeatedly low-pass the waveform, then put every value back inside the
// +-0.5 interval its 8-bit code allows. The result never moves further from
// any sample than the original rounding did, so transients and real detail
// stay; smooth passages lose the grain. Until a region is ready the renderer
// plays the raw bytes, so audio never waits on the worker.
struct CleanRegion {
    uint32_t lo = 0, hi = 0;           // [lo, hi) byte addresses
    std::vector<float> y;
    std::atomic<bool> ready{false};
};

class Dequantizer {
public:
    Dequantizer() {
        const char* e = std::getenv("TEMPLEFROG_GAX_DEQUANT");
        enabled_ = !(e && e[0] == '0');
    }
    ~Dequantizer() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    bool enabled() const { return enabled_; }

    // Emulation thread only. Returns the region (possibly not ready yet), or
    // null for addresses outside the ROM.
    std::shared_ptr<CleanRegion> get(const gba::MemView& mem, uint32_t lo, uint32_t hi) {
        if (!enabled_ || hi <= lo || hi - lo > (1u << 20) || lo < 0x08000000u || hi > 0x0A000000u)
            return nullptr;
        const uint64_t key = (static_cast<uint64_t>(lo) << 32) | hi;
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;
        const uint8_t* raw = mem.slice(lo, hi - lo);
        if (!raw) return nullptr;
        auto r = std::make_shared<CleanRegion>();
        r->lo = lo;
        r->hi = hi;
        if (cache_.size() > 2048) cache_.clear();   // bound memory; regions re-clean on demand
        cache_[key] = r;
        Job job{r, std::vector<int8_t>(reinterpret_cast<const int8_t*>(raw),
                                       reinterpret_cast<const int8_t*>(raw) + (hi - lo))};
        {
            std::lock_guard<std::mutex> lk(mu_);
            jobs_.push_back(std::move(job));
            if (!worker_.joinable()) worker_ = std::thread([this] { run(); });
        }
        cv_.notify_one();
        return r;
    }

private:
    struct Job { std::shared_ptr<CleanRegion> r; std::vector<int8_t> q; };

    void run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return stop_ || !jobs_.empty(); });
                if (stop_) return;
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            clean(job);
        }
    }

    static void clean(Job& job) {
        // Low-pass: 31-tap Hann-windowed sinc at 0.36 cycles/sample (72% of
        // the sample's own Nyquist): above it an 8-bit instrument is mostly
        // rounding noise.
        constexpr int kHalf = 15;
        constexpr double kCut = 0.36;
        static const std::array<float, 2 * kHalf + 1> h = [] {
            std::array<float, 2 * kHalf + 1> t{};
            double sum = 0.0;
            for (int k = -kHalf; k <= kHalf; ++k) {
                const double x = k;
                const double sinc = k == 0 ? 2.0 * kCut
                    : std::sin(2.0 * 3.14159265358979323846 * kCut * x) / (3.14159265358979323846 * x);
                const double win = 0.5 + 0.5 * std::cos(3.14159265358979323846 * x / (kHalf + 1));
                t[k + kHalf] = static_cast<float>(sinc * win);
                sum += sinc * win;
            }
            for (float& v : t) v = static_cast<float>(v / sum);
            return t;
        }();
        const std::size_t n = job.q.size();
        std::vector<float> y(n), tmp(n);
        for (std::size_t i = 0; i < n; ++i) y[i] = job.q[i];
        for (int it = 0; it < 24; ++it) {
            for (std::size_t i = 0; i < n; ++i) {
                float acc = 0.0f;
                for (int k = -kHalf; k <= kHalf; ++k) {
                    const std::ptrdiff_t j = std::clamp<std::ptrdiff_t>(
                        static_cast<std::ptrdiff_t>(i) + k, 0, static_cast<std::ptrdiff_t>(n) - 1);
                    acc += h[k + kHalf] * y[static_cast<std::size_t>(j)];
                }
                tmp[i] = acc;
            }
            for (std::size_t i = 0; i < n; ++i)
                y[i] = std::clamp(tmp[i], job.q[i] - 0.5f, job.q[i] + 0.5f);
        }
        job.r->y = std::move(y);
        job.r->ready.store(true, std::memory_order_release);
    }

    bool enabled_ = true;
    std::map<uint64_t, std::shared_ptr<CleanRegion>> cache_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::thread worker_;
    bool stop_ = false;
};

// Live voice in the float renderer: position in source samples, as a byte
// address plus a fraction, moving +1 (forward) or -1 (reverse) per sample.
struct LiveVoice {
    Voice v;
    double pos = 0.0;      // absolute byte address + fraction
    double end = 0.0;
    double start = 0.0;    // where this note began: never read before it
    double step = 0.0;     // source samples per guest (8192 Hz) sample
    int dir = 1;
    float gain_l = 0.0f, gain_r = 0.0f;   // current (ramped) gains
    float d_gain_l = 0.0f, d_gain_r = 0.0f;
    float fade = 1.0f, d_fade = 0.0f;     // note-on fade-in / cut-off fade-out
    int ramp_left = 0;
    bool done = false;
    bool leaving = false;  // gone from the engine: fading out
    bool seen = false;
    std::shared_ptr<CleanRegion> clean;   // de-quantised bytes, once ready
};

// Band-limited interpolation: a 16-tap Lanczos-windowed sinc (a = 8),
// tabulated over 1024 sub-sample phases. Upsampling (the usual case: the
// samples are a quarter of the output rate or less) uses the table at the
// source's own Nyquist, which removes the images cubic interpolation leaves;
// a pitched-up voice widens the kernel into an anti-aliasing filter.
constexpr int kTaps = 8;          // each side
constexpr int kPhases = 1024;
struct SincTable {
    float w[kPhases + 1][2 * kTaps];
    SincTable() {
        for (int ph = 0; ph <= kPhases; ++ph) {
            const double frac = static_cast<double>(ph) / kPhases;
            double sum = 0.0;
            for (int k = 0; k < 2 * kTaps; ++k) {
                const double x = (k - (kTaps - 1)) - frac;
                double v;
                if (std::fabs(x) < 1e-9) v = 1.0;
                else if (std::fabs(x) >= kTaps) v = 0.0;
                else {
                    const double px = 3.14159265358979323846 * x;
                    v = std::sin(px) / px * std::sin(px / kTaps) / (px / kTaps);
                }
                w[ph][k] = static_cast<float>(v);
                sum += v;
            }
            for (int k = 0; k < 2 * kTaps; ++k) w[ph][k] = static_cast<float>(w[ph][k] / sum);
        }
    }
};
const SincTable& sinc_table() { static const SincTable t; return t; }

class GaxHq final : public gba::ExternalDirectSource {
public:
    GaxHq() {
        const char* e = std::getenv("TEMPLEFROG_GAX_VERIFY");
        verify_ = e && *e && *e != '0';
        const char* g = std::getenv("TEMPLEFROG_GAX_DEBUG");
        debug_ = g && *g && *g != '0';
        // TEMPLEFROG_GAX_WAV=<prefix>: A/B capture of the native mix and the
        // hardware-equivalent stream (verify predictions), written at exit.
        if (const char* w = std::getenv("TEMPLEFROG_GAX_WAV"); w && *w) {
            wav_prefix_ = w;
            verify_ = true;
        }
    }

    std::size_t hook_pcs(uint32_t* out, std::size_t max) const override {
        if (max == 0) return 0;
        out[0] = kChunkMixer;
        return 1;
    }

    void reset() override {
        queue_.clear();
        live_.clear();
        started_ = false;
        chunk_pos_ = 0;
        fallback_ = false;
        lim_fill_ = 0;
        lim_gain_ = 1.0f;
    }

    // Transparent look-ahead limiter. The engine's mix runs hot and the
    // hardware hard-clips it at +-127 (heard as crackle once the rest is
    // clean). Watch the next ~2 ms: when a peak would cross the ceiling, ease
    // the gain down just ahead of it (~1 ms) and recover over ~80 ms; a soft
    // knee backs it up. Adds ~2 ms of latency.
    void limit(uint32_t rate, float& l, float& r) {
        constexpr float kCeiling = 118.0f;   // s8 units, a little under 127
        const int la = std::clamp(static_cast<int>(rate / 500), 8, kLimMax - 1);   // ~2 ms
        lim_l_[lim_pos_] = l;
        lim_r_[lim_pos_] = r;
        const int out = (lim_pos_ + kLimMax - la) % kLimMax;
        lim_pos_ = (lim_pos_ + 1) % kLimMax;
        float peak = 0.0f;
        for (int k = 0, i = lim_pos_; k < la; ++k) {
            i = (i + kLimMax - 1) % kLimMax;
            peak = std::max(peak, std::max(std::fabs(lim_l_[i]), std::fabs(lim_r_[i])));
        }
        const float target = peak > kCeiling ? kCeiling / peak : 1.0f;
        const float attack = 1.0f - std::exp(-1.0f / (0.001f * rate));
        const float release = 1.0f - std::exp(-1.0f / (0.080f * rate));
        lim_gain_ += (target - lim_gain_) * (target < lim_gain_ ? attack : release);
        if (lim_fill_ < la) { ++lim_fill_; l = r = 0.0f; return; }   // priming the delay
        auto knee = [](float x) {   // soft clip above 110, never past 127
            const float a = std::fabs(x);
            if (a <= 110.0f) return x;
            const float y = 110.0f + 17.0f * std::tanh((a - 110.0f) / 17.0f);
            return x < 0.0f ? -y : y;
        };
        l = knee(lim_l_[out] * lim_gain_);
        r = knee(lim_r_[out] * lim_gain_);
    }

    void on_hook(uint32_t pc, bool, const gba::MemView& mem) override {
        if ((pc & ~1u) != kChunkMixer) return;
        mem_ = mem;
        ++hooks_;
        uint32_t s = 0;
        if (!mem.u32(kStatePtr, s) || s < 0x03000000u || s >= 0x03008000u) return;
        uint8_t count = 0, fx_enabled = 0;
        mem.u8(s + 0x00, count);
        mem.u8(s + 0x10E, fx_enabled);
        uint32_t table = 0;
        mem.u32(s + 0xC4, table);
        Chunk chunk;
        for (int i = 0; i < std::min<int>(count, kMaxVoices); ++i) {
            uint32_t p = 0;
            if (!mem.u32(table + 4u * i, p) || p == 0) continue;
            const uint8_t* raw = mem.slice(p, 28);
            if (!raw) continue;
            Voice v;
            v.addr = p;
            mem.u32(p + 0x04, v.cur);
            mem.u32(p + 0x08, v.end);
            mem.u32(p + 0x0C, v.loop_start);
            mem.u32(p + 0x10, v.loop_len);
            v.step = rd16(mem, p + 0x14);
            v.frac = rd16(mem, p + 0x16);
            v.vol_l = raw[0x18];
            v.vol_r = raw[0x19];
            v.invert = raw[0x1A];
            v.fx = raw[0x1B];
            if (v.fx && fx_enabled) chunk.fx_active = true;
            chunk.voices.push_back(v);
        }
        if (verify_) verify(mem, s, chunk);
        queue_.push_back(std::move(chunk));
        while (queue_.size() > 6) queue_.pop_front();   // never lag far behind
    }

    bool render(uint32_t rate, float& fifo_a, float& fifo_b) override {
        if (rate == 0) return false;
        const int per_chunk = static_cast<int>(std::lround(kChunk * rate / kGuestRate));
        if (!started_) {
            // One chunk of latency, like the hardware's ring.
            if (queue_.size() < 2) return false;
            started_ = true;
            chunk_pos_ = per_chunk;
        }
        if (chunk_pos_ >= per_chunk) {
            chunk_pos_ = 0;
            if (!queue_.empty()) {
                load(queue_.front(), per_chunk);
                queue_.pop_front();
            }   // else keep extrapolating the current voices until the next one
        }
        ++chunk_pos_;
        ++rendered_;
        if (fallback_) { ++fallback_samples_; return false; }
        const double scale = kGuestRate / rate;   // guest samples per output sample
        float left = 0.0f, right = 0.0f;
        for (LiveVoice& lv : live_) {
            if (lv.done) continue;
            float x = sample(lv, lv.step * scale) * lv.fade;
            if (!lv.v.loop_start) {
                // A one-shot that ends on a non-zero value would stop dead
                // (a click the hardware's crunch used to hide): taper its
                // last ~2 ms of source to silence.
                const double remain = (lv.end - lv.pos) * lv.dir;   // source samples left
                const double taper = std::max(4.0, lv.step * kGuestRate * 0.002);
                if (remain < taper) x *= static_cast<float>(std::max(0.0, remain / taper));
            }
            left += x * lv.gain_l;
            right += x * lv.gain_r;
            if (lv.ramp_left > 0) {
                lv.gain_l += lv.d_gain_l;
                lv.gain_r += lv.d_gain_r;
                --lv.ramp_left;
            }
            if (lv.d_fade != 0.0f) {
                lv.fade += lv.d_fade;
                if (lv.fade >= 1.0f) { lv.fade = 1.0f; lv.d_fade = 0.0f; }
                if (lv.fade <= 0.0f) { lv.fade = 0.0f; lv.d_fade = 0.0f; if (lv.leaving) lv.done = true; }
            }
            advance(lv, lv.step * scale);
        }
        limit(rate, left, right);
        fifo_a = left;    // FIFO A = left, FIFO B = right (SOUNDCNT_H routes them)
        fifo_b = right;
        if (!wav_prefix_.empty()) {
            wav_rate_ = rate;
            wav_native_.push_back(static_cast<int16_t>(std::clamp(left * 256.0f, -32767.0f, 32767.0f)));
            wav_native_.push_back(static_cast<int16_t>(std::clamp(right * 256.0f, -32767.0f, 32767.0f)));
        }
        return true;
    }

    void log_summary(const char* when) const override {
        std::fprintf(stderr,
            "[templefrog] native mixer (%s): chunk hooks %llu, rendered %.1f s, fallback %.1f%%",
            when, static_cast<unsigned long long>(hooks_),
            rendered_ / 32768.0,
            rendered_ ? 100.0 * fallback_samples_ / rendered_ : 0.0);
        if (!wav_prefix_.empty() && std::string(when) == "exit") write_wavs();
        if (verify_)
            std::fprintf(stderr, ", verify chunks %llu exact %llu (slot +%d), differing bytes %llu",
                         static_cast<unsigned long long>(verified_),
                         static_cast<unsigned long long>(exact_), slot_offset_,
                         static_cast<unsigned long long>(diff_bytes_));
        std::fprintf(stderr, "\n");
    }

private:
    void load(const Chunk& c, int per_chunk) {
        fallback_ = c.fx_active;
        for (LiveVoice& lv : live_) lv.seen = false;
        const int ramp = std::max(1, per_chunk);
        const int fade_in = std::max(1, per_chunk / 16);    // ~1 ms
        const int fade_out = std::max(1, per_chunk / 5);    // ~3 ms
        for (const Voice& v : c.voices) {
            const int dir = (static_cast<int32_t>(v.end - v.cur) < 0) ? -1 : 1;
            const double snap = static_cast<double>(v.cur) + dir * (v.frac / 4096.0);
            float gl = v.vol_l ? (v.vol_l + 1) / 256.0f : 0.0f;
            float gr = v.vol_r ? (v.vol_r + 1) / 256.0f : 0.0f;
            if (v.invert) gr = -gr;
            // The same slot still playing the same sample near where we are:
            // keep our own continuous position (the engine's integer stepping
            // differs by a fraction) and ramp to the new gains.
            LiveVoice* same = nullptr;
            for (LiveVoice& lv : live_)
                if (!lv.done && !lv.leaving && !lv.seen && lv.v.addr == v.addr &&
                    lv.v.loop_start == v.loop_start && lv.dir == dir &&
                    std::fabs(lv.pos - snap) < 4.0) { same = &lv; break; }
            if (!same && debug_) {
                for (const LiveVoice& lv : live_)
                    if (!lv.done && !lv.leaving && lv.v.addr == v.addr)
                        std::fprintf(stderr, "[gaxdbg] resync t=%.4f slot=%08X ours=%.2f engine=%.2f loop=%08X/%08X dir=%d/%d\n",
                                     rendered_ / 32768.0, v.addr, lv.pos, snap, lv.v.loop_start, v.loop_start, lv.dir, dir);
            }
            if (same) {
                same->seen = true;
                same->v = v;
                same->end = static_cast<double>(v.end);
                same->step = v.step / 4096.0;
                same->d_gain_l = (gl - same->gain_l) / ramp;
                same->d_gain_r = (gr - same->gain_r) / ramp;
                same->ramp_left = ramp;
                continue;
            }
            // A new note (or the slot was re-triggered): start at the
            // engine's position with a 1 ms fade-in so it cannot click.
            LiveVoice lv;
            lv.v = v;
            lv.dir = dir;
            lv.pos = snap;
            lv.start = static_cast<double>(v.cur);
            lv.end = static_cast<double>(v.end);
            lv.step = v.step / 4096.0;
            lv.gain_l = gl;
            lv.gain_r = gr;
            lv.fade = 0.0f;
            lv.d_fade = 1.0f / fade_in;
            lv.seen = true;
            if (debug_)
                std::fprintf(stderr, "[gaxdbg] note t=%.4f slot=%08X cur=%08X first=%d gl=%.3f gr=%.3f\n",
                             rendered_ / 32768.0, v.addr, v.cur, s8(mem_, v.cur), gl, gr);
            {
                // The bytes this note can touch: from its start to the end
                // of its sample (or loop), in playing order.
                uint32_t lo, hi;
                if (dir > 0) {
                    lo = v.cur;
                    hi = std::max(v.end, v.loop_start ? v.loop_start + v.loop_len : v.end);
                } else {
                    hi = v.cur + 1;
                    lo = std::min(v.end, v.loop_start ? v.loop_start - v.loop_len : v.end);
                }
                lv.clean = dequant_.get(mem_, lo, hi);
                // Use it for the whole note or not at all: switching mid-note
                // would itself be a (tiny) step.
                if (lv.clean && !lv.clean->ready.load(std::memory_order_acquire)) lv.clean.reset();
            }
            live_.push_back(lv);
        }
        // Voices the engine dropped before their sample ended were cut off:
        // fade them out over ~3 ms instead of stopping dead.
        for (LiveVoice& lv : live_)
            if (!lv.seen && !lv.done && !lv.leaving) {
                lv.leaving = true;
                lv.d_fade = -lv.fade / fade_out;
            }
        live_.erase(std::remove_if(live_.begin(), live_.end(),
                                   [](const LiveVoice& lv) { return lv.done; }),
                    live_.end());
    }

    // Byte at sample index i along the voice's direction from its base,
    // following the loop like the guest (one-shot: hold the last sample).
    float fetch(const LiveVoice& lv, int64_t a) const {
        const int64_t end = static_cast<int64_t>(lv.end);
        const int64_t start = static_cast<int64_t>(lv.start);
        if (lv.start != 0.0 && (lv.dir > 0 ? a < start : a > start) && !lv.v.loop_start)
            return 0.0f;   // before the note began
        if (lv.dir > 0 ? a >= end : a <= end) {
            if (lv.v.loop_start && lv.v.loop_len) {
                const int64_t over = lv.dir > 0 ? a - end : end - a;
                a = static_cast<int64_t>(lv.v.loop_start) + lv.dir * (over % lv.v.loop_len);
            } else {
                return 0.0f;   // past a one-shot's end
            }
        }
        if (lv.clean && a >= lv.clean->lo && a < lv.clean->hi)
            return lv.clean->y[static_cast<std::size_t>(a - lv.clean->lo)];
        return static_cast<float>(s8(mem_, static_cast<uint32_t>(a)));
    }

    // Band-limited interpolation along the voice's direction. `ratio` is
    // source samples per output sample: <= 1 uses the tabulated kernel at
    // the source Nyquist; > 1 (pitched up) stretches it to anti-alias.
    float sample(const LiveVoice& lv, double ratio) const {
        const double base = lv.dir > 0 ? std::floor(lv.pos) : std::ceil(lv.pos);
        const double t = std::fabs(lv.pos - base);
        const int64_t b = static_cast<int64_t>(base);
        if (ratio <= 1.0) {
            const float* w = sinc_table().w[static_cast<int>(t * kPhases + 0.5)];
            float acc = 0.0f;
            for (int k = 0; k < 2 * kTaps; ++k)
                acc += w[k] * fetch(lv, b + lv.dir * (k - (kTaps - 1)));
            return acc;
        }
        const double fc = 1.0 / ratio;
        const int n = std::min(48, static_cast<int>(std::ceil(kTaps * ratio)));
        double acc = 0.0, norm = 0.0;
        for (int k = -n + 1; k <= n; ++k) {
            const double x = (k - t) * fc;
            double v;
            if (std::fabs(x) < 1e-9) v = 1.0;
            else if (std::fabs(x) >= kTaps) v = 0.0;
            else {
                const double px = 3.14159265358979323846 * x;
                v = std::sin(px) / px * std::sin(px / kTaps) / (px / kTaps);
            }
            acc += v * fetch(lv, b + lv.dir * k);
            norm += v;
        }
        return static_cast<float>(norm != 0.0 ? acc / norm : 0.0);
    }

    void advance(LiveVoice& lv, double by) {
        lv.pos += lv.dir * by;
        const bool past = lv.dir > 0 ? lv.pos >= lv.end : lv.pos <= lv.end;
        if (!past) return;
        if (!lv.v.loop_start) { lv.done = true; return; }
        const double over = lv.dir > 0 ? lv.pos - lv.end : lv.end - lv.pos;
        lv.pos = static_cast<double>(lv.v.loop_start) + lv.dir * over;
        lv.end = static_cast<double>(lv.v.loop_start) + lv.dir * static_cast<double>(lv.v.loop_len);
        if (lv.dir > 0 ? lv.pos >= lv.end : lv.pos <= lv.end)
            lv.pos = static_cast<double>(lv.v.loop_start);
    }

    // The guest's exact integer mixer for one chunk (forward voices; reverse
    // voices are mirrored), then pack/clamp to s8 like the packer.
    void predict(const Chunk& c, Prediction& out) const {
        std::array<int32_t, kChunk> l{}, r{};
        for (Voice v : c.voices) {
            if (v.fx) continue;   // FX-bus voices are not in the main bus
            const int dir = (static_cast<int32_t>(v.end - v.cur) < 0) ? -1 : 1;
            const int32_t gl = v.vol_l ? v.vol_l + 1 : 0;
            const int32_t gr = (v.vol_r ? v.vol_r + 1 : 0) * (v.invert ? -1 : 1);
            uint32_t cur = v.cur, end = v.end;
            uint32_t frac = v.frac;
            for (int i = 0; i < kChunk; ++i) {
                const int32_t s0 = s8(mem_, cur);
                int32_t s1;
                const bool at_end = dir > 0 ? cur + 1 >= end : cur - 1 <= end;
                if (at_end) s1 = v.loop_start ? s8(mem_, v.loop_start) : s0;
                else s1 = s8(mem_, cur + dir);
                const int32_t x = s0 + (((s1 - s0) * static_cast<int32_t>(frac)) >> 12);
                l[i] += (x * gl) >> 8;
                r[i] += (x * gr) >> 8;
                frac += v.step;
                if (frac >= 0x1000) {
                    cur += dir * static_cast<int32_t>(frac >> 12);
                    frac &= 0xFFF;
                }
                const bool past = dir > 0 ? static_cast<int32_t>(cur - end) >= 0
                                          : static_cast<int32_t>(end - cur) >= 0;
                if (past) {
                    if (!v.loop_start) break;
                    cur = cur - end + v.loop_start;
                    end = v.loop_start + dir * static_cast<int32_t>(v.loop_len);
                    const bool still = dir > 0 ? static_cast<int32_t>(cur - end) >= 0
                                               : static_cast<int32_t>(end - cur) >= 0;
                    if (still) cur = v.loop_start;
                }
            }
        }
        for (int i = 0; i < kChunk; ++i) {
            out.a[i] = static_cast<int8_t>(std::clamp(l[i], -127, 127));
            out.b[i] = static_cast<int8_t>(std::clamp(r[i], -127, 127));
        }
    }

    void verify(const gba::MemView& mem, uint32_t s, const Chunk& chunk) {
        // Compare last chunk's prediction with what the packer wrote. The
        // slot it used relative to the index at entry is learned once.
        if (pending_.valid) {
            int best = -1;
            uint64_t best_diff = ~0ull;
            for (int off = 0; off < 3; ++off) {
                const uint32_t slot = (pending_.idx + off) % 3;
                const uint8_t* ga = mem.slice(pending_.ring_a + slot * kChunk, kChunk);
                const uint8_t* gb = mem.slice(pending_.ring_b + slot * kChunk, kChunk);
                if (!ga || !gb) continue;
                uint64_t d = 0;
                for (int i = 0; i < kChunk; ++i)
                    d += (static_cast<int8_t>(ga[i]) != pending_.a[i]) +
                         (static_cast<int8_t>(gb[i]) != pending_.b[i]);
                if (d < best_diff) { best_diff = d; best = off; }
            }
            if (best >= 0) {
                ++verified_;
                if (slot_offset_ < 0) slot_offset_ = best;
                if (best_diff == 0) ++exact_;
                diff_bytes_ += best_diff;
            }
            if (verified_ && verified_ % 512 == 0) log_summary("verify");
        }
        if (pending_.valid && !wav_prefix_.empty())
            for (int i = 0; i < kChunk; ++i) {   // hardware-equivalent stream at 8192 Hz
                wav_hw_.push_back(static_cast<int16_t>(pending_.a[i] * 256));
                wav_hw_.push_back(static_cast<int16_t>(pending_.b[i] * 256));
            }
        pending_.valid = !chunk.fx_active;
        mem.u32(s + 0xE4, pending_.ring_a);
        mem.u32(s + 0xE8, pending_.ring_b);
        mem.u8(s + 0xF0, pending_.idx);
        if (pending_.valid) predict(chunk, pending_);
    }

    static void write_wav(const std::string& path, const std::vector<int16_t>& lr, uint32_t rate) {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return;
        const uint32_t data = static_cast<uint32_t>(lr.size() * 2), riff = 36 + data;
        const uint16_t fmt = 1, ch = 2, bits = 16, align = 4;
        const uint32_t fmt_len = 16, bps = rate * 4;
        std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
        std::fwrite(&fmt_len, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f);
        std::fwrite(&rate, 4, 1, f); std::fwrite(&bps, 4, 1, f); std::fwrite(&align, 2, 1, f);
        std::fwrite(&bits, 2, 1, f); std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
        std::fwrite(lr.data(), 2, lr.size(), f);
        std::fclose(f);
        std::fprintf(stderr, "[templefrog] wrote %s\n", path.c_str());
    }
    void write_wavs() const {
        if (!wav_native_.empty()) write_wav(wav_prefix_ + "_native.wav", wav_native_, wav_rate_);
        if (!wav_hw_.empty()) write_wav(wav_prefix_ + "_hardware.wav", wav_hw_, 8192);
    }

    std::string wav_prefix_;
    std::vector<int16_t> wav_native_, wav_hw_;
    uint32_t wav_rate_ = 32768;
    static constexpr int kLimMax = 512;
    std::array<float, kLimMax> lim_l_{}, lim_r_{};
    int lim_pos_ = 0, lim_fill_ = 0;
    float lim_gain_ = 1.0f;
    gba::MemView mem_{};
    Dequantizer dequant_;
    std::deque<Chunk> queue_;
    std::vector<LiveVoice> live_;
    bool started_ = false;
    bool fallback_ = false;
    bool debug_ = false;
    int chunk_pos_ = 0;
    uint64_t hooks_ = 0, rendered_ = 0, fallback_samples_ = 0;
    // verification
    bool verify_ = false;
    Prediction pending_;
    uint64_t verified_ = 0, exact_ = 0, diff_bytes_ = 0;
    int slot_offset_ = -1;
};

GaxHq g_gax_hq;

}  // namespace

gba::ExternalDirectSource* templefrog_gax_hq_source() { return &g_gax_hq; }
