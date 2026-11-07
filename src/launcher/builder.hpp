#pragma once
// Builds a game's code on the player's PC from their own disc: checks the disc, copies its files
// out (the game then plays from those, without the image), translates the boot executable to C++
// with the bundled eerecomp/vurecomp, and compiles the result into game.dll with the bundled clang.
// Nothing of the game ships with the runner.

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace launcher {

namespace fs = std::filesystem;

// Where everything lives, relative to the folder holding the executable.
struct Paths {
    fs::path root;      // the executable's folder: game.dll goes here
    fs::path compiler;  // bundled toolchain, translators, runtime headers and game config
    fs::path game;      // what the build makes from the disc (boot executable, C++, objects)
    fs::path data;      // the disc's files, copied out of the image by the build: the game plays from these
    fs::path saves;
    fs::path logs;
    static Paths from_exe();
};

// compiler/config/game.txt: key=value lines describing the one disc release this build is for.
struct GameConfig {
    std::map<std::string, std::string> values;
    std::string get(const std::string &key) const;
    static bool load(const fs::path &file, GameConfig &out, std::string &error);
};

struct Progress {
    std::atomic<float> fraction{0.0f};
    std::atomic<bool> running{false}, done{false}, failed{false}, cancel{false};
    void status(const std::string &s);
    void line(const std::string &s);
    std::string status() const;
    std::vector<std::string> lines() const;

private:
    mutable std::mutex mutex_;
    std::string status_;
    std::vector<std::string> lines_;
};

// Opens the disc image and checks its boot executable against the config; on success fills
// boot (the executable's file name on the disc).
bool check_disc(const Paths &paths, const GameConfig &config, const std::string &iso, std::string &boot, std::string &error);

// Opens the disc's files the build copied out (paths.data) and checks them like a disc: true when
// they are all there, at their sizes, and hold the configured release. Playing and rebuilding need
// only these, not the image.
bool check_data(const Paths &paths, const GameConfig &config, std::string &error);

// True when game.dll exists and was built for this runner from the configured disc release.
bool is_built(const Paths &paths, const GameConfig &config);

// The whole build; reports through progress and writes logs/build.log. It works from the copied
// files when they check out, else from the disc image iso, copying its files out first.
bool build(const Paths &paths, const GameConfig &config, const std::string &iso, Progress &progress);

// Developer tool: writes the recipe that rebuilds recorded VU1 microcode images out of the boot
// executable's bytes, so the images themselves never need to ship.
int write_vu_recipe(const fs::path &elf, const fs::path &vu1_dir, const fs::path &out);

// Developer tool: checks the disc against config_file and writes its boot executable to out_dir.
int extract_boot(const fs::path &config_file, const std::string &iso, const fs::path &out_dir);

// Settings the launcher keeps in settings.ini beside the executable.
struct Settings {
    std::string iso;
    bool fullscreen = false;
    bool widescreen = false;
    int scale = 1;
    static Settings load(const Paths &paths);
    void save(const Paths &paths) const;
};

}
