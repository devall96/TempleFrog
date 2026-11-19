// TempleFrog: builds the game from the player's own ROM and BIOS.
//
// A TempleFrog release carries no code made from the game: the player's ROM
// (and BIOS) are recompiled with the bundled gba_recompile and compiled with
// the bundled compiler into two game modules, which the runtime loads at start
// (gbarecomp/docs/GAME_MODULES.md). This happens once, on the first start or in
// RecompLauncher's setup step, and again only when TempleFrog's code generator
// changes or the player picks a different file.
#pragma once

#include <functional>
#include <string>

enum class TfModule { Cart, Bios };

// Where the parts live, relative to the program's folder.
struct TfBuildPaths {
    std::string exe_dir;  // TempleFrog's folder (writable: the AppImage runs from a copy)
    std::string kit_dir;  // <exe_dir>/gamekit: recompiler, module SDK, compiler
    std::string out_dir;  // <exe_dir>/native: the modules and their stamps
    std::string config;   // game.toml the game runs with (working directory, else exe_dir)
};
TfBuildPaths tf_build_paths(const std::string& exe_dir);

// True when this TempleFrog can build modules (the game kit is there).
bool tf_build_kit_present(const TfBuildPaths& paths);

// Path of a module, and whether it exists and was built by this game kit from
// the file with this SHA-1.
std::string tf_module_path(const TfBuildPaths& paths, TfModule which);
bool tf_module_current(const TfBuildPaths& paths, TfModule which, const std::string& sha1);

// Progress: fraction 0..1 of this build and a short line of text (already in
// the player's language).
using TfProgress = std::function<void(float fraction, const std::string& text)>;

// Recompiles `source_path` (the ROM or BIOS, already verified to have SHA-1
// `sha1`) and compiles it into the module. Returns true when the module is in
// place; otherwise false with `error` set. A log of the run is left in
// <out_dir>/build.log. Safe to run from a worker thread.
bool tf_build_module(const TfBuildPaths& paths, TfModule which, const std::string& source_path,
                     const std::string& sha1, const TfProgress& progress, std::string* error);

// Checks the game kit works on this machine without a ROM: compiles a stand-in
// module (one function, SHA-1 "selftest") into <out_dir>/selftest and returns
// its path, or an empty string with `error` set. For CI and bug reports
// (TempleFrog --kit-selftest).
std::string tf_kit_selftest(const TfBuildPaths& paths, std::string* error);

// The retail GBA BIOS, the only one the BIOS module is built from (gba_bios.toml
// describes that dump). With another dump the runtime's BIOS replacements (HLE)
// stand in, as they do in any build without the recompiled BIOS.
constexpr const char* kTfRetailBiosSha1 = "300c20df6731a33952ded8c436f7f186d25d3492";
