#pragma once

#include <string>

namespace launcher {

// The launcher window (choose disc, build, settings, play).
int run();
// The same build without a window: builds from iso, printing progress to stderr.
int build_cli(const std::string &iso);

}
