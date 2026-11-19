// TempleFrog: log file and the checks a --game start makes before booting.
#pragma once

#include <string>

// Exit codes for a --game start (RecompLauncher shows a message for each; see
// recomplauncher.json "exit_codes"). Without --game none of these are used.
enum TempleFrogExit {
    kExitRomMissing = 20,      // the ROM file could not be opened
    kExitZipWithoutRom = 21,   // a .zip with no .gba file inside
    kExitEuropeanRom = 22,     // the European release (AFRP); needs the USA one
    kExitWrongRom = 23,        // some other file
    kExitBiosMissing = 24,     // no BIOS, and no way to ask for one (Big Picture)
    kExitBuildFailed = 25,     // building the game from the ROM failed (see native/build.log)
    kExitKitMissing = 26,      // the game kit (recompiler and compiler) is missing
    kExitBuildClosed = 27,     // the player closed the window while the game was being built
};

// Sends stdout and stderr to <exe dir>/logs/templefrog.log, keeping the last
// five runs (templefrog.1.log ... templefrog.4.log), when the program has no
// console or redirect of its own (started from Explorer or a launcher) or runs
// under RecompLauncher. A console or a redirect keeps its output as before.
void templefrog_start_log(const std::string& exe_dir);

// For a --game start, before the game boots: reads the ROM (a .gba, or a .zip
// holding one, which is unpacked into roms/ and `rom_path` pointed at it),
// checks it is the USA release, and in Big Picture mode makes sure a BIOS is in
// place. Returns 0, or one of the exit codes above after printing why.
// `rom_sha1`, when given, gets the ROM's SHA-1 (lower-case hex).
int templefrog_check_game(std::string& rom_path, const std::string& exe_dir,
                          std::string* rom_sha1 = nullptr);

// The player's BIOS: game.toml's [bios] path, else the one they picked
// (bios.cfg). Empty when there is none yet.
std::string templefrog_find_bios(const std::string& exe_dir);

// SHA-1 of a file (lower-case hex), empty when it can't be read.
std::string templefrog_file_sha1(const std::string& path);
