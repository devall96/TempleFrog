// TempleFrog's game-specific player options, run as gbarecomp's per-frame
// input policy (RunOptions::input_frame) plus two rows in the in-game menu.
//
//  - Game > Language: the cartridge asks for a language (English, Francais,
//    Deutsch, Espanol, Italiano) every time you press Start on the title
//    screen. Pick one here and TempleFrog answers that screen by pressing the
//    same buttons a player would (Up/Down, then A).
//  - Game > Rumble: controller rumble on deaths, 1-ups, elements and coins.
//
// Both only read game memory, and steer the game only through key presses, so
// saves, replays and save states stay exactly as the game makes them. Input
// replays suspend the policy.
#pragma once

#include "runtime.h"

#include <string>

// Reads the saved choices (templefrog.ini next to the exe) and wires the menu
// rows and the policy into opts. Call before run_game().
void templefrog_policy_install(gbarecomp::RunOptions& opts, const std::string& exe_dir);
