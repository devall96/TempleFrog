#include "boxart.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

// Title screen with no input: Konami logo ~f300, "Press Start" by f900.
constexpr int kTitleFrame = 900;
// Nearest-neighbour upscale so the launcher's texture filtering keeps pixels crisp.
constexpr int kArtScale = 4;

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    std::size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    s = s.substr(b);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    return s;
}

std::string first_line(const fs::path& p) {
    std::ifstream f(p);
    std::string line;
    if (f && std::getline(f, line)) return trim(line);
    return {};
}

// Value of `key = "..."` under [section]; resolved relative to the toml's dir.
std::string toml_path(const fs::path& toml, const char* section, const char* key) {
    std::ifstream f(toml);
    std::string line, cur;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t[0] == '[') { cur = t.substr(1, t.find(']') - 1); continue; }
        if (cur != section) continue;
        const std::size_t eq = t.find('=');
        if (eq == std::string::npos || trim(t.substr(0, eq)) != key) continue;
        std::string v = t.substr(eq + 1);
        const std::size_t a = v.find('"'), b = v.rfind('"');
        if (a == std::string::npos || b <= a) return {};
        fs::path p = v.substr(a + 1, b - a - 1);
        if (p.is_relative()) p = toml.parent_path() / p;
        return p.string();
    }
    return {};
}

std::uint32_t rd32(const std::vector<std::uint8_t>& d, std::size_t o) {
    return d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (std::uint32_t(d[o + 3]) << 24);
}

// Top-down RGB888 image decoded from the runtime's 24-bit --dump-bmp.
struct Rgb { int w = 0, h = 0; std::vector<std::uint8_t> px; };

bool read_bmp(const fs::path& bmp, Rgb& img) {
    std::ifstream in(bmp, std::ios::binary);
    std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(in)), {});
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
    const std::uint32_t off = rd32(d, 10);
    const std::int32_t w = std::int32_t(rd32(d, 18));
    std::int32_t h = std::int32_t(rd32(d, 22));
    const std::uint16_t bpp = std::uint16_t(d[28] | (d[29] << 8));
    if (bpp != 24 || w <= 0 || h == 0) return false;
    const bool bottom_up = h > 0;
    if (h < 0) h = -h;
    const std::size_t row = (std::size_t(w) * 3 + 3) & ~std::size_t(3);
    if (d.size() < off + row * std::size_t(h)) return false;
    img.w = w; img.h = h; img.px.resize(std::size_t(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        const std::size_t src = off + row * std::size_t(bottom_up ? (h - 1 - y) : y);
        for (int x = 0; x < w; ++x) {   // BGR -> RGB
            img.px[(std::size_t(y) * w + x) * 3 + 0] = d[src + std::size_t(x) * 3 + 2];
            img.px[(std::size_t(y) * w + x) * 3 + 1] = d[src + std::size_t(x) * 3 + 1];
            img.px[(std::size_t(y) * w + x) * 3 + 2] = d[src + std::size_t(x) * 3 + 0];
        }
    }
    return true;
}

// RGB888 title frame -> 24-bit TGA, upscaled.
bool rgb_to_tga(const Rgb& img, const fs::path& tga, int scale) {
    const int w = img.w, h = img.h;

    const int W = w * scale, H = h * scale;
    std::vector<std::uint8_t> out;
    out.reserve(18 + std::size_t(W) * H * 3);
    const std::uint8_t hdr[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  std::uint8_t(W), std::uint8_t(W >> 8),
                                  std::uint8_t(H), std::uint8_t(H >> 8), 24, 0x20};  // top-left origin
    out.insert(out.end(), hdr, hdr + 18);
    for (int y = 0; y < H; ++y) {
        const std::size_t src_row = std::size_t(y / scale) * w * 3;
        for (int x = 0; x < W; ++x) {
            const std::size_t s = src_row + std::size_t(x / scale) * 3;
            out.push_back(img.px[s + 2]);   // TGA stores BGR
            out.push_back(img.px[s + 1]);
            out.push_back(img.px[s]);
        }
    }
    fs::create_directories(tga.parent_path());
    std::ofstream o(tga, std::ios::binary);
    o.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    return bool(o);
}

#if defined(_WIN32)
fs::path exe_path() {
    std::wstring buf(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
    buf.resize(n);
    return fs::path(buf);
}

std::wstring quote(const fs::path& p) { return L"\"" + p.wstring() + L"\""; }

bool run_hidden(const std::wstring& cmd, const fs::path& cwd, DWORD timeout_ms) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring mutable_cmd = cmd;
    SetEnvironmentVariableW(L"GBARECOMP_DIAGNOSTICS", L"0");
    SetEnvironmentVariableW(L"TEMPLEFROG_NO_BOXART", L"1");
    const BOOL ok = CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                                   CREATE_NO_WINDOW, nullptr, cwd.wstring().c_str(), &si, &pi);
    SetEnvironmentVariableW(L"GBARECOMP_DIAGNOSTICS", nullptr);
    SetEnvironmentVariableW(L"TEMPLEFROG_NO_BOXART", nullptr);
    if (!ok) return false;
    const bool done = WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_OBJECT_0;
    DWORD code = 1;
    if (done) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return done && code == 0;
}
#endif

}  // namespace

void templefrog_ensure_boxart() {
#if defined(_WIN32)
    if (const char* e = std::getenv("TEMPLEFROG_NO_BOXART"); e && *e && *e != '0') return;
    std::error_code ec;
    const fs::path exe = exe_path();
    const fs::path dir = exe.parent_path();
    const fs::path art = dir / "assets" / "img" / "boxart.tga";
    const fs::path backdrop = dir / "assets" / "img" / "backdrop.tga";  // blurred launcher backdrop
    if (fs::exists(art, ec) && fs::exists(backdrop, ec)) return;

    // Same lookup as the launcher seam: cwd, beside the exe, one level up.
    fs::path toml;
    for (const fs::path& c : {fs::current_path(ec) / "game.toml", dir / "game.toml",
                              dir.parent_path() / "game.toml"})
        if (fs::exists(c, ec)) { toml = c; break; }
    if (toml.empty()) return;

    // The player's own picks win (launcher sidecars), then game.toml.
    auto from_cfg = [&](const char* name) -> fs::path {
        std::string v = first_line(dir / name);
        if (v.empty()) return {};
        fs::path p = v;
        if (p.is_relative()) p = dir / p;
        return fs::exists(p, ec) ? p : fs::path{};
    };
    fs::path rom = from_cfg("rom.cfg");
    if (rom.empty()) rom = toml_path(toml, "rom", "path");
    fs::path bios = from_cfg("bios.cfg");
    if (bios.empty()) bios = toml_path(toml, "bios", "path");
    if (rom.empty() || !fs::exists(rom, ec) || bios.empty() || !fs::exists(bios, ec)) return;

    const fs::path tmp = fs::temp_directory_path(ec) / "templefrog-boxart";
    fs::create_directories(tmp, ec);
    const fs::path bmp = tmp / "title.bmp";
    const fs::path scratch_save = tmp / "scratch.sav";  // never the player's save
    fs::remove(bmp, ec);
    fs::remove(scratch_save, ec);

    const std::wstring cmd = quote(exe) + L" --no-window --frames " + std::to_wstring(kTitleFrame) +
        L" --dump-bmp " + quote(bmp) + L" --rom " + quote(rom) + L" --bios " + quote(bios) +
        L" --save-path " + quote(scratch_save) + L" " + quote(toml);
    Rgb title;
    if (run_hidden(cmd, toml.parent_path(), 30000) && read_bmp(bmp, title)) {
        if (!fs::exists(art, ec) && rgb_to_tga(title, art, kArtScale))
            std::fprintf(stderr, "[templefrog] box art created from your ROM's title screen: %s\n",
                         art.string().c_str());
        // Launcher backdrop: the same frame, heavily blurred (three box-blur
        // passes) so it reads as soft jungle colour, not legible title art.
        Rgb soft = title;
        for (int pass = 0; pass < 3; ++pass) {
            const int r = 6;
            Rgb tmp = soft;
            for (int y = 0; y < soft.h; ++y)          // horizontal
                for (int x = 0; x < soft.w; ++x)
                    for (int c = 0; c < 3; ++c) {
                        int sum = 0, n = 0;
                        for (int k = -r; k <= r; ++k) {
                            const int xx = std::clamp(x + k, 0, soft.w - 1);
                            sum += soft.px[(std::size_t(y) * soft.w + xx) * 3 + c]; ++n;
                        }
                        tmp.px[(std::size_t(y) * soft.w + x) * 3 + c] = std::uint8_t(sum / n);
                    }
            for (int y = 0; y < soft.h; ++y)          // vertical
                for (int x = 0; x < soft.w; ++x)
                    for (int c = 0; c < 3; ++c) {
                        int sum = 0, n = 0;
                        for (int k = -r; k <= r; ++k) {
                            const int yy = std::clamp(y + k, 0, soft.h - 1);
                            sum += tmp.px[(std::size_t(yy) * soft.w + x) * 3 + c]; ++n;
                        }
                        soft.px[(std::size_t(y) * soft.w + x) * 3 + c] = std::uint8_t(sum / n);
                    }
        }
        (void)rgb_to_tga(soft, backdrop, 2);
    }
    fs::remove(bmp, ec);
    fs::remove(scratch_save, ec);
#endif
}
