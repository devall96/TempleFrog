// TempleFrog's game-specific player options (see game_policy.h).
//
// Evidence for the addresses (USA ROM, AFRE):
//  - gGameStuff.mode at IWRAM 0x03005339 (JRickey/frog-adv-temple-decomp,
//    include/constants/game_mode.h). Modes 8..23 are the scene engine (levels);
//    29 is the world map. The language screen after Press Start is mode 27
//    (measured: title 4, language screen 27, main menu 4).
//  - The language cursor is IWRAM 0x03003494 (0..4 in the order on screen),
//    found by diffing IWRAM with the cursor on rows 0, 1 and 2. Confirming
//    writes the language to 0x030034B0.
//  - HUD run-state gIwram_35E0 at 0x030035E0 (decomp docs/game-model.md):
//    lives u8 at +0, coins u16 at +2, elementsCollected at +4. There is no
//    health: any hazard kills, so a "hit" is a death.
#include "game_policy.h"

#include "host_overlay.h"
#include "host_window.h"
#include "touch_input.h"
#if __has_include("recomp_runtime_ui.h")
#include "recomp_runtime_ui.h"
#define TEMPLEFROG_HAVE_MENU 1
#endif
#if __has_include("launcher_i18n.h")
#include "launcher_i18n.h"
#define TEMPLEFROG_HAVE_I18N 1
#endif

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>

void apply_ui_language();

// Menu and overlay text in the player's language (launcher_i18n).
static const char* ui_tr(const char* english) {
#if defined(TEMPLEFROG_HAVE_I18N)
    return launcher_i18n_text(english);
#else
    return english;
#endif
}

namespace {

constexpr std::uint32_t kModeOffset = 0x5339;     // IWRAM 0x03005339
constexpr std::uint32_t kCursorOffset = 0x3494;   // IWRAM 0x03003494
constexpr std::uint32_t kRunStateOffset = 0x35E0; // IWRAM 0x030035E0
constexpr std::uint8_t kLanguageScreenMode = 27;
constexpr std::uint16_t kKeyA = 1u << 0, kKeyStart = 1u << 3;
constexpr std::uint16_t kKeyUp = 1u << 6, kKeyDown = 1u << 7;
constexpr const char* kLanguageKey = "templefrog.language";
constexpr const char* kRumbleKey = "templefrog.rumble";
constexpr const char* kTimerKey = "templefrog.speedrun_timer";

// 0 = let the game ask; 1..5 = English, Francais, Deutsch, Espanol, Italiano.
std::atomic<int> g_language{0};
std::atomic<int> g_rumble{0};
std::atomic<int> g_timer{0};
std::string g_ini_path;

const char* const kLanguageChoices[] = {
    "Ask every time", "English", "Fran\xC3\xA7" "ais", "Deutsch",
    "Espa\xC3\xB1" "ol", "Italiano",
};

void save_settings() {
    if (g_ini_path.empty()) return;
    std::ofstream out(g_ini_path, std::ios::trunc);
    out << "[TempleFrog]\nlanguage=" << g_language.load()
        << "\nrumble=" << g_rumble.load()
        << "\nspeedrun_timer=" << g_timer.load() << "\n";
}

void load_settings() {
    std::ifstream in(g_ini_path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("language=", 0) == 0) {
            const int v = std::atoi(line.c_str() + 9);
            if (v >= 0 && v <= 5) g_language.store(v);
        } else if (line.rfind("rumble=", 0) == 0) {
            g_rumble.store(std::atoi(line.c_str() + 7) != 0 ? 1 : 0);
        } else if (line.rfind("speedrun_timer=", 0) == 0) {
            g_timer.store(std::atoi(line.c_str() + 15) != 0 ? 1 : 0);
        }
    }
}

// ---- language screen ------------------------------------------------------
int g_settle = 0;   // frames spent on the language screen so far

std::uint16_t language_keys(const gbarecomp::TouchFrameInfo* f, std::uint8_t mode) {
    const int language = g_language.load(std::memory_order_relaxed);
    if (!language || mode != kLanguageScreenMode) {
        g_settle = 0;
        return 0;
    }
    // Let the screen fade in, then press one button every 8 frames so each
    // press is a fresh edge and the cursor has settled before the next.
    if (g_settle < 20) {
        ++g_settle;
        return 0;
    }
    if ((f->frame_count & 7u) != 0) return 0;
    const int cursor = f->iwram[kCursorOffset];
    const int target = language - 1;
    if (cursor > 4) return 0;
    if (cursor < target) return kKeyDown;
    if (cursor > target) return kKeyUp;
    return kKeyA;
}

// ---- rumble -----------------------------------------------------------------
struct RunState {
    bool valid = false;
    std::uint64_t frame = 0;
    std::uint8_t mode = 0;
    std::uint8_t lives = 0;
    std::uint16_t coins = 0;
    std::uint8_t elements = 0;
};
RunState g_last;

bool gameplay_mode(std::uint8_t mode) { return mode >= 8 && mode <= 23; }

void rumble_events(const gbarecomp::TouchFrameInfo* f, std::uint8_t mode) {
    const std::uint8_t* rs = f->iwram + kRunStateOffset;
    RunState now;
    now.valid = true;
    now.frame = f->frame_count;
    now.mode = mode;
    now.lives = rs[0];
    now.coins = static_cast<std::uint16_t>(rs[2] | (rs[3] << 8));
    now.elements = rs[4];
    // Only compare consecutive frames of the same gameplay scene: level
    // set-up, mode changes, state loads and rewinds (frame jumps) resync
    // silently.
    const bool comparable = g_last.valid && gameplay_mode(mode) &&
                            g_last.mode == mode && now.frame == g_last.frame + 1;
    static const bool log = [] {
        const char* e = std::getenv("TEMPLEFROG_RUMBLE_LOG");
        return e && e[0] && e[0] != '0';
    }();
    if (comparable && (log || g_rumble.load(std::memory_order_relaxed))) {
        const char* what = nullptr;
        int ms = 0;
        float strength = 0.0f;
        if (now.lives < g_last.lives)            { what = "death"; ms = 320; strength = 0.85f; }
        else if (now.lives > g_last.lives)       { what = "1-up"; ms = 140; strength = 0.5f; }
        else if (now.elements > g_last.elements) { what = "element"; ms = 110; strength = 0.45f; }
        else if (now.coins > g_last.coins)       { what = "coin"; ms = 45; strength = 0.22f; }
        if (what) {
            if (g_rumble.load(std::memory_order_relaxed))
                gbarecomp::host_haptic_pulse(ms, strength);
            if (log)
                std::fprintf(stderr, "[templefrog] rumble %s frame=%llu lives=%u coins=%u elements=%u\n",
                             what, static_cast<unsigned long long>(now.frame), now.lives,
                             now.coins, now.elements);
        }
    }
    g_last = now;
}

// ---- speedrun timer ---------------------------------------------------------
// Counts guest frames (59.7275 Hz), so host pauses, menus and slowdowns never
// add time. A level's time runs while a scene-engine mode (8..23) is active;
// leaving it records a split. Loads and rewinds (frame jumps) neither add nor
// subtract time.
struct Timer {
    std::uint64_t last_frame = 0;
    bool have_frame = false;
    bool in_level = false;
    std::uint64_t level_frames = 0;
    std::uint64_t total_frames = 0;
    std::uint64_t last_split = 0;
    int splits = 0;
};
Timer g_run;

void timer_tick(const gbarecomp::TouchFrameInfo* f, std::uint8_t mode) {
    const bool consecutive = g_run.have_frame && f->frame_count == g_run.last_frame + 1;
    g_run.have_frame = true;
    g_run.last_frame = f->frame_count;
    const bool level = gameplay_mode(mode);
    if (level && !g_run.in_level) {
        g_run.in_level = true;
        g_run.level_frames = 0;
    } else if (!level && g_run.in_level) {
        g_run.in_level = false;
        g_run.last_split = g_run.level_frames;
        ++g_run.splits;
    }
    if (level && consecutive) {
        ++g_run.level_frames;
        ++g_run.total_frames;
    }
}

void format_time(std::uint64_t frames, char* out, std::size_t n) {
    const double seconds = static_cast<double>(frames) * 280896.0 / 16777216.0;
    const auto cs = static_cast<std::uint64_t>(seconds * 100.0);
    const auto h = cs / 360000, m = cs / 6000 % 60, sec = cs / 100 % 60, c = cs % 100;
    if (h)
        std::snprintf(out, n, "%llu:%02llu:%02llu.%02llu", (unsigned long long)h,
                      (unsigned long long)m, (unsigned long long)sec, (unsigned long long)c);
    else
        std::snprintf(out, n, "%llu:%02llu.%02llu", (unsigned long long)m,
                      (unsigned long long)sec, (unsigned long long)c);
}

// Host overlay, every present (guest thread, after the game image).
void draw_timer(gbarecomp::HostOverlay* o) {
    if (!g_timer.load(std::memory_order_relaxed) || !o || !o->text_supported()) return;
    char level[32], total[32], split[32], l1[48], l2[48], l3[48];
    format_time(g_run.level_frames, level, sizeof level);
    format_time(g_run.total_frames, total, sizeof total);
    std::snprintf(l1, sizeof l1, "%s %s", ui_tr("Level"), level);
    std::snprintf(l2, sizeof l2, "%s %s", ui_tr("Total"), total);
    const bool has_split = g_run.splits > 0;
    if (has_split) {
        format_time(g_run.last_split, split, sizeof split);
        std::snprintf(l3, sizeof l3, "%s %s", ui_tr("Last"), split);
    }
    const auto game = o->game_rect();
    const float size = std::max(14.0f, static_cast<float>(game.height) / 30.0f);
    const float pad = size * 0.4f;
    float width = std::max(o->text_width(l1, size, gbarecomp::OverlaySpace::Drawable),
                           o->text_width(l2, size, gbarecomp::OverlaySpace::Drawable));
    if (has_split)
        width = std::max(width, o->text_width(l3, size, gbarecomp::OverlaySpace::Drawable));
    const int lines = has_split ? 3 : 2;
    const float box_w = width + pad * 2.0f, box_h = lines * size * 1.15f + pad * 2.0f;
    const float x = static_cast<float>(game.x + game.width) - box_w - pad;
    // Bottom-right: the game's HUD uses the top corners (lives, coins) and the
    // bottom-left (elements).
    const float y = static_cast<float>(game.y + game.height) - box_h - pad;
    o->fill_rect(x, y, box_w, box_h, size * 0.3f, {0x20, 0x10, 0x10, 0xC0},
                 gbarecomp::OverlaySpace::Drawable);
    const gbarecomp::OverlayColor lime{0xA0, 0xE0, 0x10, 0xFF}, cream{0xF0, 0xF0, 0xC0, 0xFF};
    float ty = y + pad;
    o->text(x + pad, ty, size, lime, l1, gbarecomp::OverlayAlign::Left, gbarecomp::OverlaySpace::Drawable);
    ty += size * 1.15f;
    o->text(x + pad, ty, size, cream, l2, gbarecomp::OverlayAlign::Left, gbarecomp::OverlaySpace::Drawable);
    if (has_split) {
        ty += size * 1.15f;
        o->text(x + pad, ty, size, cream, l3, gbarecomp::OverlayAlign::Left, gbarecomp::OverlaySpace::Drawable);
    }
}

// Once per frame at VBlank start, on the guest thread. Returns the keys to
// press, active-low (0x03FF = none); the runtime ANDs them with the player's.
std::uint16_t policy(const gbarecomp::TouchFrameInfo* f) {
    if (!f || !f->iwram) return 0x03FF;
    std::uint16_t press = 0;
    // Test knob: TEMPLEFROG_PRESS_START_AT=<frame> presses Start for 6 frames
    // (headless checks of this policy; input replay would suspend it).
    static const long press_start_at = [] {
        const char* e = std::getenv("TEMPLEFROG_PRESS_START_AT");
        return e && e[0] ? std::atol(e) : -1L;
    }();
    if (press_start_at >= 0 &&
        f->frame_count >= static_cast<std::uint64_t>(press_start_at) &&
        f->frame_count < static_cast<std::uint64_t>(press_start_at) + 6)
        press |= kKeyStart;
    // Test knob: TEMPLEFROG_POLICY_TRACE=<gbarecomp-keyinput-v1 file> plays a
    // key trace through this policy (GBARECOMP_INPUT_REPLAY suspends it).
    static const std::map<std::uint64_t, std::uint16_t> trace = [] {
        std::map<std::uint64_t, std::uint16_t> t;
        const char* e = std::getenv("TEMPLEFROG_POLICY_TRACE");
        if (!e || !e[0]) return t;
        std::ifstream in(e);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            const auto comma = line.find(',');
            if (comma == std::string::npos) continue;
            t[std::strtoull(line.c_str(), nullptr, 10)] =
                static_cast<std::uint16_t>(std::strtoul(line.c_str() + comma + 1, nullptr, 0));
        }
        return t;
    }();
    if (!trace.empty()) {
        auto it = trace.upper_bound(f->frame_count);
        if (it != trace.begin()) press |= static_cast<std::uint16_t>(~(--it)->second & 0x03FFu);
    }
    const std::uint8_t mode = f->iwram[kModeOffset];
    press |= language_keys(f, mode);
    rumble_events(f, mode);
    timer_tick(f, mode);
    return static_cast<std::uint16_t>(0x03FFu & ~press);
}

#if defined(TEMPLEFROG_HAVE_MENU)
RecompRuntimeUiItem g_items[3];

int ui_get(const char* key, int* out) {
    if (std::strcmp(key, kLanguageKey) == 0) { *out = g_language.load(); return 1; }
    if (std::strcmp(key, kRumbleKey) == 0) { *out = g_rumble.load(); return 1; }
    if (std::strcmp(key, kTimerKey) == 0) { *out = g_timer.load(); return 1; }
    return 0;
}

int ui_set(const char* key, int value) {
    if (std::strcmp(key, kLanguageKey) == 0) {
        if (value >= 0 && value <= 5) {
            g_language.store(value);
            save_settings();
            apply_ui_language();   // the menus follow the game's language
        }
        return 1;
    }
    if (std::strcmp(key, kRumbleKey) == 0) {
        g_rumble.store(value != 0 ? 1 : 0);
        save_settings();
        if (value) gbarecomp::host_haptic_pulse(140, 0.5f);   // a taste of it
        return 1;
    }
    if (std::strcmp(key, kTimerKey) == 0) {
        g_timer.store(value != 0 ? 1 : 0);
        save_settings();
        return 1;
    }
    return 0;
}
#endif

}  // namespace

// The launcher and the in-game menu use the game's language: the one picked in
// Game > Language, or on "Ask every time" the system language when the game
// offers it (English otherwise).
void apply_ui_language() {
#if defined(TEMPLEFROG_HAVE_I18N)
    static const char* const kCodes[] = {"en", "fr", "de", "es", "it"};
    const int language = g_language.load();
    launcher_i18n_set_language(language >= 1 && language <= 5 ? kCodes[language - 1]
                                                               : launcher_i18n_system_language());
#endif
}

void templefrog_policy_install(gbarecomp::RunOptions& opts, const std::string& exe_dir) {
    g_ini_path = (std::filesystem::path(exe_dir) / "templefrog.ini").string();
    load_settings();
    apply_ui_language();
#if defined(TEMPLEFROG_HAVE_MENU)
    RecompRuntimeUiItem& lang = g_items[0];
    lang = RecompRuntimeUiItem{};
    lang.key = kLanguageKey;
    lang.section = "Game";
    lang.label = "Language";
    lang.description = "The game asks for a language each time you press Start. Pick one here and TempleFrog answers for you. The menus follow this language too.";
    lang.type = RECOMP_RUNTIME_UI_CHOICE;
    lang.minimum = 0;
    lang.maximum = 5;
    lang.step = 1;
    lang.choices = kLanguageChoices;
    lang.choice_count = sizeof(kLanguageChoices) / sizeof(kLanguageChoices[0]);

    RecompRuntimeUiItem& rumble = g_items[1];
    rumble = RecompRuntimeUiItem{};
    rumble.key = kRumbleKey;
    rumble.section = "Game";
    rumble.label = "Rumble";
    rumble.description = "Controller rumble when Frogger loses a life, gets a 1-up, or picks up an element or a coin.";
    rumble.type = RECOMP_RUNTIME_UI_BOOL;

    RecompRuntimeUiItem& timer = g_items[2];
    timer = RecompRuntimeUiItem{};
    timer.key = kTimerKey;
    timer.section = "Game";
    timer.label = "Speedrun timer";
    timer.description = "Shows the current level's time, the total time spent in levels, and the last level's split. Counts game frames, so pausing never adds time.";
    timer.type = RECOMP_RUNTIME_UI_BOOL;

    opts.ui_extra_items = g_items;
    opts.ui_extra_item_count = 3;
    opts.ui_get = ui_get;
    opts.ui_set = ui_set;
#endif
    opts.input_frame = policy;
    opts.host_overlay = draw_timer;
}
