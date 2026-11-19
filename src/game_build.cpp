// TempleFrog: builds the game modules from the player's ROM and BIOS (see game_build.h).
#include "game_build.h"

#include <algorithm>
#include <cerrno>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if __has_include("launcher_i18n.h")
#include "launcher_i18n.h"
#define TF_HAVE_I18N 1
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
constexpr const char* kExe = ".exe";
constexpr const char* kLib = ".dll";
#else
constexpr const char* kExe = "";
constexpr const char* kLib = ".so";
#endif

// Bump when the way modules are compiled changes in a way the game kit's id
// would not show (flags, glue): every player's modules are then rebuilt.
constexpr const char* kBuildRecipe = "recipe-1";

const char* tf_text(const char* english) {
#if defined(TF_HAVE_I18N)
    return launcher_i18n_text(english);
#else
    return english;
#endif
}

std::string module_stem(TfModule which) {
    return which == TfModule::Cart ? "templefrog-cart" : "templefrog-bios";
}

std::string read_text(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

std::string stamp_text(const TfBuildPaths& paths, const std::string& sha1) {
    return trim(read_text(fs::u8path(paths.kit_dir) / "KIT_ID")) + "\n" + kBuildRecipe + "\n" +
           sha1 + "\n";
}

// ---- running the recompiler and the compiler, without a console window ----

#if defined(_WIN32)
std::wstring widen(const std::string& s) { return fs::u8path(s).wstring(); }

std::wstring quote_arg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') out.append(slashes * 2 + 1, L'\\');
        else out.append(slashes, L'\\');
        slashes = 0;
        out.push_back(c);
    }
    out.append(slashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}
#endif

// Runs argv[0] with the arguments, its output going to `log_path`. Returns its
// exit code, or -1 when it could not start.
int run_tool(const std::vector<std::string>& argv, const fs::path& log_path) {
#if defined(_WIN32)
    std::wstring cmd;
    for (const auto& a : argv) {
        if (!cmd.empty()) cmd.push_back(L' ');
        cmd += quote_arg(widen(a));
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = CreateFileW(log_path.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return -1;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = log;
    si.hStdError = log;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    const BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                                   nullptr, &si, &pi);
    CloseHandle(log);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(code);
#else
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 1, log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&fa, 1, 2);
    std::vector<char*> args;
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, args[0], &fa, nullptr, args.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0) return -1;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    SetEnvironmentVariableW(widen(name).c_str(), widen(value).c_str());
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

std::string path_string(const fs::path& p) {
    const auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

void append_file(std::ofstream& to, const fs::path& from, const std::string& title) {
    to << "---- " << title << " ----\n" << read_text(from) << "\n";
}

// The last lines of a tool's output, for the error the player sees.
std::string tail_lines(const fs::path& p, int count) {
    std::string text = trim(read_text(p));
    size_t pos = text.size();
    for (int i = 0; i < count && pos != std::string::npos && pos > 0; ++i)
        pos = text.rfind('\n', pos - 1);
    return pos == std::string::npos ? text : text.substr(pos + 1);
}

}  // namespace

TfBuildPaths tf_build_paths(const std::string& exe_dir) {
    TfBuildPaths p;
    p.exe_dir = exe_dir;
    // The AppImage runs the kit straight from its read-only image (AppRun).
    const char* kit = std::getenv("TEMPLEFROG_KIT_DIR");
    p.kit_dir = kit && kit[0] ? std::string(kit) : path_string(fs::u8path(exe_dir) / "gamekit");
    p.out_dir = path_string(fs::u8path(exe_dir) / "native");
    std::error_code ec;
    const fs::path here = fs::absolute("game.toml", ec);
    p.config = !ec && fs::exists(here, ec) ? path_string(here)
                                           : path_string(fs::u8path(exe_dir) / "game.toml");
    return p;
}

bool tf_build_kit_present(const TfBuildPaths& paths) {
    std::error_code ec;
    const fs::path kit = fs::u8path(paths.kit_dir);
    return fs::exists(kit / "KIT_ID", ec) &&
           fs::exists(kit / (std::string("gba_recompile") + kExe), ec) &&
           fs::exists(kit / "zig" / (std::string("zig") + kExe), ec);
}

std::string tf_module_path(const TfBuildPaths& paths, TfModule which) {
    return path_string(fs::u8path(paths.out_dir) / (module_stem(which) + kLib));
}

bool tf_module_current(const TfBuildPaths& paths, TfModule which, const std::string& sha1) {
    std::error_code ec;
    const fs::path module = fs::u8path(tf_module_path(paths, which));
    if (!fs::exists(module, ec)) return false;
    fs::path stamp = module;
    stamp += ".stamp";
    return read_text(stamp) == stamp_text(paths, sha1);
}

namespace {

// A stand-in for recompiled code, for tf_kit_selftest: one function and a
// dispatch table, written the way gba_recompile writes them.
constexpr const char* kSelfTestSource = R"(// TempleFrog game kit self-test.
#include "runtime_arm.h"
#include "native_program.h"
extern "C" void gf_selftest_08000000(void) { g_cpu.R[0] += 1u; }
extern "C" const DispatchEntry kDispatchTable[] = {{0x08000000u, 0u, 0u, gf_selftest_08000000}};
extern "C" const unsigned kDispatchTableLen = 1;
)";

bool build_module(const TfBuildPaths& paths, TfModule which, const std::string& source_path,
                  const std::string& sha1, const TfProgress& progress, std::string* error,
                  bool selftest) {
    std::string scratch;
    std::string& err = error ? *error : scratch;
    auto report = [&](float f, const char* english) {
        if (progress) progress(f, tf_text(english));
    };
    std::error_code ec;
    const fs::path kit = fs::u8path(paths.kit_dir);
    const fs::path out = fs::u8path(paths.out_dir);
    const bool cart = which == TfModule::Cart;
    const fs::path work = out / ("work-" + module_stem(which));
    const fs::path gen = work / "generated";
    const fs::path module = fs::u8path(tf_module_path(paths, which));
    fs::path stamp = module;
    stamp += ".stamp";
    const fs::path log_path = out / "build.log";

    if (!tf_build_kit_present(paths)) {
        err = tf_text("TempleFrog's game kit is missing; reinstall TempleFrog.");
        return false;
    }
    fs::create_directories(out, ec);
    fs::remove_all(work, ec);
    fs::create_directories(gen, ec);
    if (ec) {
        err = "could not create " + path_string(gen) + ": " + ec.message();
        return false;
    }
    std::ofstream log(log_path, std::ios::app);
    log << "==== " << module_stem(which) << " from " << sha1 << "\n";
    // The compiler's own cache (its C runtime, built once) stays between
    // builds; anything made from the game stays in the work folder, which goes
    // when the build ends.
    set_env("ZIG_GLOBAL_CACHE_DIR", path_string(out / "zig-cache"));
    set_env("ZIG_LOCAL_CACHE_DIR", path_string(work / "zig-local"));

    // 1. Recompile the ROM or BIOS into C++.
    const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
    if (selftest) {
        std::ofstream(gen / "selftest.cpp", std::ios::binary) << kSelfTestSource;
    } else {
    report(0.02f, cart ? "Recompiling your ROM" : "Recompiling your BIOS");
    unsigned shards = 2;
    while (shards < cores && shards < 16) shards *= 2;
    std::vector<std::string> recompile = {path_string(kit / (std::string("gba_recompile") + kExe))};
    if (cart) {
        recompile.insert(recompile.end(),
                         {"--rom", source_path, "--config",
                          paths.config, "--max-functions",
                          "65536", "--codegen-shards", std::to_string(shards)});
    } else {
        recompile.insert(recompile.end(), {"--bios", source_path, "--config",
                                           path_string(kit / "gba_bios.toml")});
    }
    recompile.insert(recompile.end(), {"--out", path_string(gen)});
    const fs::path recompile_log = work / "recompile.log";
    const int rc = run_tool(recompile, recompile_log);
    append_file(log, recompile_log, "gba_recompile");
    if (rc != 0) {
        err = std::string(tf_text("Recompiling failed")) + " (" + std::to_string(rc) + "): " +
              tail_lines(recompile_log, 3);
        fs::remove_all(work, ec);
        return false;
    }
    }

    // 2. Compile every source, several at once, biggest first.
    struct Job {
        fs::path source;
        fs::path object;
        uintmax_t size = 0;
        std::vector<std::string> extra;
    };
    std::vector<Job> jobs;
    for (const auto& entry : fs::directory_iterator(gen, ec)) {
        if (entry.path().extension() != ".cpp") continue;
        Job j;
        j.source = entry.path();
        j.object = work / (entry.path().stem().string() + ".o");
        j.size = entry.file_size(ec);
        jobs.push_back(j);
    }
    {
        Job glue;
        glue.source = kit / "sdk" / "module_glue.cpp";
        glue.object = work / "module_glue.o";
        glue.size = 1;
        glue.extra = {cart ? "-DGBARECOMP_MODULE_CART" : "-DGBARECOMP_MODULE_BIOS",
                      "-DGBARECOMP_MODULE_SOURCE_SHA1=\"" + sha1 + "\""};
        jobs.push_back(glue);
    }
    std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) { return a.size > b.size; });
    uintmax_t total = 0;
    for (const auto& j : jobs) total += j.size;

    const std::string zig = path_string(kit / "zig" / (std::string("zig") + kExe));
#if defined(_WIN32)
    const std::string target = "x86_64-windows-gnu";
#else
    const std::string target = "x86_64-linux-gnu.2.27";
#endif
    std::vector<std::string> common = {
        // zig cc turns on UBSan checks below -O2; they would slow the game.
        zig, "c++", "-target", target, "-std=c++20", "-g0", "-w", "-fno-exceptions", "-fno-rtti",
        "-fno-sanitize=all",
        "-I" + path_string(kit / "sdk"), "-I" + path_string(gen)};
#if !defined(_WIN32)
    common.insert(common.end(), {"-fPIC", "-fvisibility=hidden"});
#endif
    if (!cart) common.push_back("-D_start=gbarecomp_bios_reset_start");

    report(0.1f, cart ? "Compiling the game" : "Compiling the BIOS");
    std::atomic<size_t> next{0};
    std::atomic<uintmax_t> done_bytes{0};
    std::atomic<bool> failed{false};
    std::mutex mutex;
    std::string first_error;
    auto worker = [&]() {
        for (;;) {
            const size_t i = next.fetch_add(1);
            if (i >= jobs.size() || failed) return;
            const Job& j = jobs[i];
            std::vector<std::string> argv = common;
            // Same optimisation as TempleFrog's own build of this code.
            const bool code = j.source.stem().string().rfind("recompiled", 0) == 0 ||
                              j.source.stem().string().rfind("bios_recompiled", 0) == 0;
            argv.push_back(code ? "-O1" : "-O2");
            argv.insert(argv.end(), j.extra.begin(), j.extra.end());
            argv.insert(argv.end(), {"-c", path_string(j.source), "-o", path_string(j.object)});
            const fs::path jlog = work / (j.source.stem().string() + ".log");
            const int code_rc = run_tool(argv, jlog);
            std::lock_guard<std::mutex> lock(mutex);
            append_file(log, jlog, j.source.filename().string());
            if (code_rc != 0) {
                if (!failed) first_error = j.source.filename().string() + ": " + tail_lines(jlog, 3);
                failed = true;
                return;
            }
            done_bytes += j.size;
            if (progress)
                progress(0.1f + 0.8f * static_cast<float>(done_bytes) / static_cast<float>(total),
                         tf_text(cart ? "Compiling the game" : "Compiling the BIOS"));
        }
    };
    std::vector<std::thread> threads;
    const unsigned parallel = std::min<unsigned>(cores, static_cast<unsigned>(jobs.size()));
    for (unsigned t = 0; t < parallel; ++t) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    if (failed) {
        err = std::string(tf_text("Compiling failed")) + ": " + first_error;
        fs::remove_all(work, ec);
        return false;
    }

    // 3. Link the module, then swap it in.
    report(0.92f, "Linking");
    fs::path temp = module;
    temp += ".new";
    std::vector<std::string> link = {zig, "c++", "-target", target, "-shared", "-nostdlib++",
                                     "-O2", "-fno-sanitize=all", "-o", path_string(temp)};
#if !defined(_WIN32)
    link.push_back("-Wl,-z,now");
#endif
    for (const auto& j : jobs) link.push_back(path_string(j.object));
    const fs::path link_log = work / "link.log";
    const int link_rc = run_tool(link, link_log);
    append_file(log, link_log, "link");
    if (link_rc != 0 || !fs::exists(temp, ec)) {
        err = std::string(tf_text("Linking failed")) + ": " + tail_lines(link_log, 3);
        fs::remove_all(work, ec);
        return false;
    }
    fs::remove(module, ec);
    fs::remove(stamp, ec);
    fs::rename(temp, module, ec);
    if (ec) {
        err = "could not write " + path_string(module) + ": " + ec.message();
        fs::remove_all(work, ec);
        return false;
    }
    // The Windows linker leaves an import library and debug info beside the
    // DLL; nothing uses them.
    for (const auto& entry : fs::directory_iterator(out, ec)) {
        const auto ext = entry.path().extension();
        if (ext == ".lib" || ext == ".pdb") fs::remove(entry.path(), ec);
    }
    {
        std::ofstream s(stamp, std::ios::binary);
        s << stamp_text(paths, sha1);
    }
    fs::remove_all(work, ec);
    report(1.0f, "Done");
    log << "==== done\n";
    return true;
}

}  // namespace

bool tf_build_module(const TfBuildPaths& paths, TfModule which, const std::string& source_path,
                     const std::string& sha1, const TfProgress& progress, std::string* error) {
    return build_module(paths, which, source_path, sha1, progress, error, false);
}

std::string tf_kit_selftest(const TfBuildPaths& paths, std::string* error) {
    TfBuildPaths p = paths;
    p.out_dir = path_string(fs::u8path(paths.out_dir) / "selftest");
    std::error_code ec;
    fs::remove_all(fs::u8path(p.out_dir), ec);
    if (!build_module(p, TfModule::Cart, std::string(), "selftest", nullptr, error, true))
        return {};
    return tf_module_path(p, TfModule::Cart);
}
