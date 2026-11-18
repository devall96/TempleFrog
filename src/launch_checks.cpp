// TempleFrog: log file and the checks a --game start makes (see launch_checks.h).
#include "launch_checks.h"

#include "../gbarecomp/src/gba/sha1.h"
#include "mod_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kUsaSha1 = "7b4c27009198df18555e63fb5dcad223eaf09815";
constexpr const char* kEuropeSha1 = "775243abe242cd748bf9a9648481efe61b93f046";

bool has_own_output() {
#if defined(_WIN32)
    const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
    return GetFileType(h) != FILE_TYPE_UNKNOWN;
#else
    return true;  // Linux/macOS: only RecompLauncher starts switch to the file.
#endif
}

bool lower_ext_is(const fs::path& p, const char* ext) {
    std::string e = p.extension().string();
    for (auto& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e == ext;
}

// [bios] path from game.toml in the working directory, if any.
std::string toml_bios_path() {
    std::ifstream in("game.toml");
    std::string line;
    bool in_bios = false;
    while (std::getline(in, line)) {
        const size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos || line[s] == '#') continue;
        if (line[s] == '[') { in_bios = line.compare(s, 6, "[bios]") == 0; continue; }
        if (!in_bios || line.compare(s, 4, "path") != 0) continue;
        const size_t a = line.find('"'), b = line.rfind('"');
        if (a != std::string::npos && b > a) return line.substr(a + 1, b - a - 1);
    }
    return {};
}

bool bios_available(const std::string& exe_dir) {
    std::error_code ec;
    const std::string from_toml = toml_bios_path();
    if (!from_toml.empty() && fs::exists(from_toml, ec)) return true;
    std::ifstream cfg(fs::path(exe_dir) / "bios.cfg");   // the picker's remembered choice
    std::string cached;
    if (std::getline(cfg, cached)) {
        while (!cached.empty() && (cached.back() == '\r' || cached.back() == ' ')) cached.pop_back();
        fs::path p(cached);
        if (p.is_relative()) p = fs::path(exe_dir) / p;
        if (!cached.empty() && fs::exists(p, ec)) return true;
    }
    return false;
}

int fail(int code, const char* message) {
    std::fprintf(stderr, "[templefrog] %s (exit %d)\n", message, code);
    std::fflush(stderr);
    return code;
}

}  // namespace

void templefrog_start_log(const std::string& exe_dir) {
    const char* rl = std::getenv("RECOMPLAUNCHER");
    if (has_own_output() && !(rl && rl[0] == '1')) return;
    std::error_code ec;
    const fs::path dir = fs::path(exe_dir) / "logs";
    fs::create_directories(dir, ec);
    if (ec) return;
    // Keep the last five runs.
    for (int i = 3; i >= 0; --i) {
        const fs::path from = dir / (i ? "templefrog." + std::to_string(i) + ".log" : "templefrog.log");
        const fs::path to = dir / ("templefrog." + std::to_string(i + 1) + ".log");
        if (fs::exists(from, ec)) fs::rename(from, to, ec);
    }
    const std::string path = (dir / "templefrog.log").string();
    if (!std::freopen(path.c_str(), "w", stdout)) return;
#if defined(_WIN32)
    _dup2(_fileno(stdout), _fileno(stderr));
#else
    dup2(fileno(stdout), fileno(stderr));
#endif
    std::setvbuf(stdout, nullptr, _IOLBF, 4096);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
}

int templefrog_check_game(std::string& rom_path, const std::string& exe_dir) {
    std::ifstream in(fs::u8path(rom_path), std::ios::binary);
    if (!in) return fail(kExitRomMissing, "could not open the ROM");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (lower_ext_is(fs::u8path(rom_path), ".zip") ||
        (bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 3 && bytes[3] == 4)) {
        std::vector<uint8_t> rom;
        std::string name, error;
        if (!gbarecomp::mod_runtime_zip_read_file(bytes, ".gba", rom, &name, &error))
            return fail(kExitZipWithoutRom, ("no usable .gba ROM in the zip: " + error).c_str());
        // Unpack next to the game's other files, so the runtime reads a plain ROM.
        std::error_code ec;
        const fs::path dir = fs::path(exe_dir) / "roms";
        fs::create_directories(dir, ec);
        const fs::path out = dir / fs::u8path(name).filename();
        std::ifstream existing(out, std::ios::binary);
        const std::vector<uint8_t> have((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
        existing.close();
        if (have != rom) {
            std::ofstream file(out, std::ios::binary | std::ios::trunc);
            if (!file.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size())))
                return fail(kExitRomMissing, "could not unpack the ROM from the zip");
        }
        std::fprintf(stderr, "[templefrog] using %s from the zip\n", name.c_str());
        const auto u8 = out.u8string();
        rom_path.assign(u8.begin(), u8.end());
        bytes.swap(rom);
    }

    const std::string sha = gba::sha1(bytes.data(), bytes.size()).hex();
    if (sha == kEuropeSha1)
        return fail(kExitEuropeanRom, "this is the European release; TempleFrog needs the USA one (AFRE)");
    if (sha != kUsaSha1)
        return fail(kExitWrongRom, "this ROM isn't Frogger's Adventures: Temple of the Frog (USA)");

    const char* mode = std::getenv("RECOMPLAUNCHER_MODE");
    if (mode && std::strcmp(mode, "bigpicture") == 0 && !bios_available(exe_dir))
        return fail(kExitBiosMissing, "no GBA BIOS: put yours in bios/gba_bios.bin in the game's folder");
    if (!bios_available(exe_dir))
        std::fprintf(stderr, "[templefrog] no GBA BIOS yet: TempleFrog will ask for yours "
                             "(or put it in bios/gba_bios.bin)\n");
    return 0;
}
