#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "gax_hq.h"
#include "game_policy.h"
#include "runtime.h"

#if defined(GBAGAME_RECOMP_UI)
#include "boxart.h"
#include "game_launcher_boot.h"
#endif

namespace {

void print_usage() {
    std::printf("TempleFrog [--no-launcher] [--game <rom>] [--bios <path>] [--rom <path>] [game.toml]\n"
                "  --game <rom>  start straight into the game with this ROM, using the saved settings\n");
}

}  // namespace

// One save folder: the battery save is saves/frog_us.sav (game.toml); save
// states and the suspend state go to saves/states instead of next to the ROM,
// so backing up or syncing (Steam Cloud, Syncthing) is one folder. Paths are
// relative to the folder holding game.toml, which is the working directory by
// the time the game starts. An explicit --state-dir wins.
static void use_save_folder(std::vector<std::string>& args) {
    for (const auto& a : args)
        if (a == "--state-dir") return;
    std::error_code ec;
    std::filesystem::create_directories("saves/states", ec);
    args.insert(args.begin() + (args.empty() ? 0 : 1), {"--state-dir", "saves/states"});
}

// --game <path> (or --game=<path>): start straight into the game with this ROM,
// for launchers (RecompLauncher), scripts and shortcuts. Removed from args and
// returned; empty when not given, which leaves every other start as it was.
static std::string take_game_arg(std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--game" && i + 1 < args.size()) {
            std::string path = args[i + 1];
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                       args.begin() + static_cast<std::ptrdiff_t>(i + 2));
            return path;
        }
        if (args[i].rfind("--game=", 0) == 0) {
            std::string path = args[i].substr(7);
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i));
            return path;
        }
    }
    return {};
}

static bool has_arg(const std::vector<std::string>& args, const char* prefix) {
    for (size_t i = 1; i < args.size(); ++i)
        if (args[i].rfind(prefix, 0) == 0) return true;
    return false;
}

// With --game: skip TempleFrog's own launcher screen the usual way
// (--no-launcher still applies the player's saved settings), run from the
// program's folder when the working directory isn't it, and open full screen
// when RecompLauncher starts the game from its Big Picture mode.
static void prepare_game_start(std::vector<std::string>& args, const char* argv0) {
    std::error_code ec;
    if (!std::filesystem::exists("game.toml", ec) && argv0 && argv0[0]) {
        const std::filesystem::path dir = std::filesystem::absolute(argv0, ec).parent_path();
        if (!ec && std::filesystem::exists(dir / "game.toml", ec))
            std::filesystem::current_path(dir, ec);
    }
    if (!has_arg(args, "--launcher") && !has_arg(args, "--no-launcher"))
        args.insert(args.begin() + 1, "--no-launcher");
    const char* mode = std::getenv("RECOMPLAUNCHER_MODE");
    if (mode && std::strcmp(mode, "bigpicture") == 0 && !has_arg(args, "--fullscreen") &&
        !has_arg(args, "--window"))
        args.insert(args.begin() + 1, "--fullscreen=1");
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    gbarecomp::RunOptions opts;
    opts.builtin_game_name = "Frogger's Adventures: Temple of the Frog";
    opts.builtin_rom_sha1 = "7b4c27009198df18555e63fb5dcad223eaf09815";
    opts.builtin_rom_crc32 = 0x2EB9EEE6u;
    opts.mod_game_id = "frogger-adventures-temple-of-the-frog-us";

    // Launcher identity and save location (game.toml [rom]/[bios]/[save]).
    opts.launcher_region = "USA";
    opts.launcher_game_version = GBARECOMP_GAME_VERSION;
    opts.launcher_game_config = "game.toml";
    opts.launcher_save_path = "saves/frog_us.sav";

    // "Temple at dusk": every colour comes from the game's own art (title-screen
    // jungle wood, logo cream and lime, level-1 leaf greens, sandstone, the
    // stained-glass blue). Contrast checked to WCAG AA in
    // docs/MENU_DESIGN_RESEARCH.md section 5.3.
    opts.ui_theme =
        "gba"
        ";background=#201010;background2=#302010"
        ";panel=#302010;panel_hovered=#3A2410"
        ";control=#402010;control_hovered=#503018;border=#4E7A14"
        ";accent=#A0E010;accent_dim=#60B000;accent_text=#201010;accent2=#A0F090"
        ";text=#F0F0C0;text_muted=#C8B088;good=#90F0D0;warn=#E0A080"
        ";focus_ring=#D0F080;scanlines=0"
        ";row_height=52;font_body=20;font_title=40;font_small=14";

    // Settings > INSTALL: desktop shortcut and Start menu / applications menu.
    // The icon is TempleFrog's own original art (art/icon), staged next to the exe.
    opts.desktop_entry_id = "templefrog";
    opts.desktop_entry_name = "TempleFrog";
    opts.desktop_entry_comment = "Frogger's Adventures: Temple of the Frog, recompiled for PC";
    opts.desktop_entry_icon_ico = "assets/img/icon.ico";
    opts.desktop_entry_icon_png = "assets/img/icon.png";

    // Native high-quality mixer for the game's sound engine ("Enhanced
    // audio" in the in-game menu's Audio tab).
    opts.audio_direct_source = templefrog_gax_hq_source();

    // Video surface. Widescreen needs game-specific view work first, so it
    // stays hidden; the renderer filters are generic and safe to offer.
    opts.launcher_expose_widescreen = false;
    // Test knob for widescreen research: allow --view-width up to 16:9
    // (284 px). Not offered to players until the margins are correct.
    if (const char* ws = std::getenv("TEMPLEFROG_WS_EXPERIMENT"); ws && ws[0] == '1')
        opts.max_view_width = 284;
    opts.launcher_expose_adaptive_view = false;
    // Let players drag the window to any size; sharp bilinear keeps
    // non-integer sizes even, integer scale letterboxes to exact multiples.
    opts.freely_resizable_window = true;
    // Fast boot: the Konami, "presents" and MusyX screens ignore the buttons.
    // With the BIOS intro skipped they end by frame ~510 and the title is up
    // by 560, so turbo through the first 540 frames.
    opts.fast_boot_frames = 540;
    // Fill the space around the picture with a blurred glow of the game
    // (Graphics > Side bars) instead of flat black.
    opts.ambient_bars_default = true;
    // Save states, fast-forward and rewind in the in-game menu. A state is
    // ~0.6 MB; one every 15 frames for 30 s keeps about 70 MB of history.
    opts.expose_assist_tools = true;
    opts.rewind_history_seconds = 30;
    opts.rewind_capture_interval_frames = 15;

    // Game > Language and Game > Rumble (src/game_policy.cpp).
    {
        std::string exe_dir = ".";
        if (argc > 0 && argv[0] && argv[0][0]) {
            const std::string a0 = argv[0];
            const auto slash = a0.find_last_of("/\\");
            if (slash != std::string::npos) exe_dir = a0.substr(0, slash);
        }
        templefrog_policy_install(opts, exe_dir);
    }
    opts.launcher_expose_sharp_filter = true;
    opts.launcher_default_sharp_filter = false;
    opts.launcher_expose_affine_filter = true;
    opts.launcher_default_affine_filter = false;

#if defined(GBAGAME_RECOMP_UI)
    // Launcher art from the player's own ROM (first launch only; no-op once made).
    templefrog_ensure_boxart();
    std::vector<std::string> args(argv, argv + argc);
    const std::string game_rom = take_game_arg(args);
    if (!game_rom.empty()) prepare_game_start(args, argv[0]);
    if (game_launcher_preboot(args, opts)) return 0;  // user closed the launcher
    if (!game_rom.empty()) args.insert(args.end(), {"--rom", game_rom});
    use_save_folder(args);
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#else
    std::vector<std::string> args(argv, argv + argc);
    const std::string game_rom = take_game_arg(args);
    if (!game_rom.empty()) {
        prepare_game_start(args, argv[0]);
        args.insert(args.end(), {"--rom", game_rom});
    }
    use_save_folder(args);
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#endif
}
