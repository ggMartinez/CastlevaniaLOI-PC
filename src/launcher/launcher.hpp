#pragma once

#include <string>

namespace launcher {

// The launcher window (choose disc, build, settings, play).
int run();
// Starts the game right away with the saved settings (--play: what the play-directly copy runs); the
// launcher window instead when the game isn't built yet.
int play_now();
// The same build without a window: builds from iso, printing progress to stderr.
int build_cli(const std::string &iso);

}
