// TempleFrog: the window shown while the game is built from the player's ROM
// (src/game_build.h). Without a display, or in a build without the launcher
// UI, progress goes to the log instead.
#pragma once

#include <functional>
#include <string>

#include "game_build.h"

// Runs `job` on a worker thread while a small window shows its progress.
// Returns the job's result. On failure the window stays up with the error (and
// where the log is) until the player closes it. `error` gets the job's error;
// it is empty when the player closed the window (TempleFrog then just quits).
bool tf_run_build_screen(const std::function<bool(const TfProgress&, std::string*)>& job,
                         const std::string& log_path, std::string* error);
