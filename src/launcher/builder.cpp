#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10: BCryptHash
#endif
#include "builder.hpp"

#include "ps2/disc.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thread>

namespace launcher {

namespace {

constexpr std::size_t kVuImageSize = 16 * 1024;

std::string utf8(const std::wstring &w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring wide(const std::string &s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

std::map<std::string, std::string> read_keys(const fs::path &file) {
    std::map<std::string, std::string> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        out[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return out;
}

bool read_file(const fs::path &file, std::vector<std::uint8_t> &out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const fs::path &file, const void *data, std::size_t size) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

std::string sha1_hex(const std::vector<std::uint8_t> &data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    unsigned char hash[20] = {};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0) return {};
    const long status = BCryptHash(alg, nullptr, 0, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), hash, sizeof(hash));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return {};
    char hex[41];
    for (int i = 0; i < 20; ++i) std::snprintf(hex + 2 * i, 3, "%02x", hash[i]);
    return std::string(hex, 40);
}

// The runner's own SHA-1: game.dll is built against this exact executable, so any rebuild of it,
// even one that only touched the runtime, makes the launcher build game.dll again.
const std::string &runner_stamp() {
    static const std::string stamp = [] {
        wchar_t exe[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::ifstream in(fs::path(std::wstring(exe, n)), std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        return sha1_hex(bytes);
    }();
    return stamp;
}

// The disc is read through _wfopen so any path works, whatever the user's code page.
bool open_disc(const std::string &iso) {
    std::FILE *f = _wfopen(wide(iso).c_str(), L"rb");
    if (!f) return false;
    _fseeki64(f, 0, SEEK_END);
    const auto size = static_cast<std::uint64_t>(_ftelli64(f));
    return ps2::disc::open(iso, size, [f](std::uint64_t offset, void *out, std::size_t bytes) {
        _fseeki64(f, static_cast<long long>(offset), SEEK_SET);
        return std::fread(out, 1, bytes, f);
    });
}

bool read_disc_file(const std::string &name, std::vector<std::uint8_t> &out) {
    const auto entry = ps2::disc::find(name);
    if (!entry || entry->directory) return false;
    const std::uint32_t sectors = (entry->size + ps2::disc::kSector - 1) / ps2::disc::kSector;
    out.resize(static_cast<std::size_t>(sectors) * ps2::disc::kSector);
    if (!ps2::disc::read(entry->sector, sectors, out.data())) return false;
    out.resize(entry->size);
    return true;
}

std::string boot_program(const std::vector<std::uint8_t> &cnf) {
    std::istringstream in(std::string(cnf.begin(), cnf.end()));
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.rfind("BOOT2", 0) != 0) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) return {};
        std::string path = trim(line.substr(eq + 1));
        const auto slash = path.find_last_of("\\/:");
        if (slash != std::string::npos) path = path.substr(slash + 1);
        const auto semi = path.find(';');
        if (semi != std::string::npos) path = path.substr(0, semi);
        return path;
    }
    return {};
}

std::wstring quote(const fs::path &p) { return L"\"" + p.wstring() + L"\""; }

// Starts a child with its output going to log (appended), no console window.
HANDLE start(const std::wstring &command, const fs::path &log) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE out = CreateFileW(log.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) return nullptr;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = out;
    si.hStdError = out;
    PROCESS_INFORMATION pi{};
    std::wstring line = command;
    const BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(out);
    if (!ok) return nullptr;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

DWORD finish(HANDLE process) {
    DWORD code = 1;
    WaitForSingleObject(process, INFINITE);
    GetExitCodeProcess(process, &code);
    CloseHandle(process);
    return code;
}

std::string tail(const fs::path &log, std::size_t lines) {
    std::ifstream in(log);
    std::vector<std::string> all;
    std::string line;
    while (std::getline(in, line)) all.push_back(line);
    std::string out;
    for (std::size_t i = all.size() > lines ? all.size() - lines : 0; i < all.size(); ++i) out += all[i] + "\n";
    return out;
}

struct VuRecipe {
    std::string file;
    struct Chunk {
        std::uint32_t vu, elf, size;
    };
    std::vector<Chunk> chunks;
};

std::vector<VuRecipe> read_vu_recipe(const fs::path &file) {
    std::vector<VuRecipe> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        VuRecipe r;
        words >> r.file;
        std::string chunk;
        while (words >> chunk) {
            VuRecipe::Chunk c{};
            if (std::sscanf(chunk.c_str(), "%x:%x:%x", &c.vu, &c.elf, &c.size) == 3) r.chunks.push_back(c);
        }
        out.push_back(r);
    }
    return out;
}

}  // namespace

Paths Paths::from_exe() {
    wchar_t exe[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    Paths p;
    p.root = fs::path(std::wstring(exe, n)).parent_path();
    p.compiler = p.root / "compiler";
    p.game = p.root / "game";
    p.saves = p.root / "saves";
    p.logs = p.root / "logs";
    return p;
}

std::string GameConfig::get(const std::string &key) const {
    const auto it = values.find(key);
    return it == values.end() ? std::string{} : it->second;
}

bool GameConfig::load(const fs::path &file, GameConfig &out, std::string &error) {
    if (!fs::exists(file)) {
        error = "missing " + utf8(file.wstring());
        return false;
    }
    out.values = read_keys(file);
    for (const char *key : {"title", "serial", "boot", "sha1"})
        if (out.get(key).empty()) {
            error = std::string("game.txt has no ") + key;
            return false;
        }
    return true;
}

void Progress::status(const std::string &s) {
    std::lock_guard lock(mutex_);
    status_ = s;
    lines_.push_back("== " + s);
}

void Progress::line(const std::string &s) {
    std::lock_guard lock(mutex_);
    lines_.push_back(s);
}

std::string Progress::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

std::vector<std::string> Progress::lines() const {
    std::lock_guard lock(mutex_);
    return lines_;
}

bool check_disc(const Paths &, const GameConfig &config, const std::string &iso, std::string &boot, std::string &error) {
    if (!open_disc(iso)) {
        error = "Cannot open the disc image.";
        return false;
    }
    std::vector<std::uint8_t> cnf, elf;
    if (!read_disc_file("SYSTEM.CNF", cnf)) {
        error = "This is not a PlayStation 2 disc image (no SYSTEM.CNF).";
        return false;
    }
    boot = boot_program(cnf);
    if (boot != config.get("boot")) {
        error = "This disc boots " + (boot.empty() ? std::string("nothing") : boot) + ", not " + config.get("boot") + ": it is another game or release. " +
                config.get("title") + " needs the " + config.get("serial") + " disc.";
        return false;
    }
    if (!read_disc_file(boot, elf) || sha1_hex(elf) != config.get("sha1")) {
        error = "The disc's " + boot + " does not match the " + config.get("serial") + " release this build is for (it may be another version or a modified copy).";
        return false;
    }
    return true;
}

bool is_built(const Paths &paths, const GameConfig &config) {
    if (!fs::exists(paths.root / "game.dll") || !fs::exists(paths.game / config.get("boot"))) return false;
    const auto stamp = read_keys(paths.game / "built.txt");
    const auto get = [&](const char *k) {
        const auto it = stamp.find(k);
        return it == stamp.end() ? std::string{} : it->second;
    };
    return get("runner") == runner_stamp() && get("sha1") == config.get("sha1");
}

bool build(const Paths &paths, const GameConfig &config, const std::string &iso, Progress &progress) {
    progress.running = true;
    progress.done = progress.failed = false;
    progress.fraction = 0.0f;
    std::error_code ec;
    fs::create_directories(paths.logs, ec);
    const fs::path log = paths.logs / "build.log";
    fs::remove(log, ec);
    const auto fail = [&](const std::string &why) {
        progress.line(why);
        progress.status("Build failed: " + why);
        progress.failed = true;
        progress.running = false;
        return false;
    };
    const auto cancelled = [&] { return progress.cancel.load() ? fail("cancelled") : false; };

    progress.status("Checking the disc");
    std::string boot, error;
    if (!check_disc(paths, config, iso, boot, error)) return fail(error);
    progress.line("disc matches " + config.get("serial") + " (" + boot + ")");
    progress.fraction = 0.02f;

    progress.status("Taking the game's executable off the disc");
    fs::remove_all(paths.game, ec);
    fs::create_directories(paths.game / "generated", ec);
    fs::create_directories(paths.game / "vu1", ec);
    fs::create_directories(paths.game / "obj", ec);
    std::vector<std::uint8_t> elf;
    if (!read_disc_file(boot, elf) || !write_file(paths.game / boot, elf.data(), elf.size())) return fail("cannot write " + boot);
    progress.fraction = 0.04f;

    progress.status("Translating the game's code to C++");
    const fs::path tools = paths.compiler;
    std::wstring cmd = quote(tools / "eerecomp.exe") + L" " + quote(paths.game / boot) + L" --entries " + quote(tools / "config" / "entries.txt") +
                       L" --code-patches " + quote(tools / "config" / "code-patches.txt") + L" --out " + quote(paths.game / "generated");
    HANDLE p = start(cmd, log);
    if (!p) return fail("cannot start eerecomp.exe");
    if (finish(p) != 0) return fail("eerecomp failed:\n" + tail(log, 15));
    if (cancelled()) return false;
    progress.fraction = 0.20f;

    progress.status("Translating the vector unit programs");
    const auto recipes = read_vu_recipe(tools / "config" / "vu1.txt");
    for (const VuRecipe &r : recipes) {
        std::vector<std::uint8_t> image(kVuImageSize, 0);
        for (const auto &c : r.chunks) {
            if (c.vu + c.size > image.size() || c.elf + c.size > elf.size()) return fail("bad VU recipe for " + r.file);
            std::memcpy(image.data() + c.vu, elf.data() + c.elf, c.size);
        }
        write_file(paths.game / "vu1" / r.file, image.data(), image.size());
    }
    if (!recipes.empty()) {
        cmd = quote(tools / "vurecomp.exe") + L" " + quote(paths.game / "vu1") + L" " + quote(paths.game / "generated");
        p = start(cmd, log);
        if (!p) return fail("cannot start vurecomp.exe");
        if (finish(p) != 0) return fail("vurecomp failed:\n" + tail(log, 15));
    }
    progress.line(std::to_string(recipes.size()) + " VU1 microcode images");
    progress.fraction = 0.22f;

    progress.status("Compiling the game code");
    std::vector<fs::path> sources;
    for (const auto &e : fs::directory_iterator(paths.game / "generated"))
        if (e.path().extension() == ".cpp") sources.push_back(e.path());
    std::sort(sources.begin(), sources.end());
    sources.push_back(tools / "ps2" / "game_entry.cpp");
    const fs::path cxx = tools / "bin" / "x86_64-w64-mingw32-clang++.exe";
    const std::wstring flags = L" -O3 -DNDEBUG -std=gnu++20 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS -DPS2_GAME_DLL"
                               L" -Wno-unused-label -Wno-unused-variable -I" + quote(paths.game / "generated") + L" -I" + quote(tools / "ps2" / "include");
    const unsigned jobs = std::max(1u, std::thread::hardware_concurrency());
    std::vector<fs::path> objects;
    std::vector<std::pair<HANDLE, fs::path>> running;
    std::size_t next = 0, finished = 0;
    bool compile_failed = false;
    while (finished < sources.size()) {
        while (!compile_failed && !progress.cancel && running.size() < jobs && next < sources.size()) {
            const fs::path &src = sources[next++];
            const fs::path obj = paths.game / "obj" / (src.stem().wstring() + L".o");
            const fs::path unit_log = paths.game / "obj" / (src.stem().wstring() + L".log");
            HANDLE h = start(quote(cxx) + flags + L" -c " + quote(src) + L" -o " + quote(obj), unit_log);
            if (!h) return fail("cannot start the bundled compiler");
            running.emplace_back(h, unit_log);
            objects.push_back(obj);
        }
        if (running.empty()) break;
        std::vector<HANDLE> handles;
        for (auto &r : running) handles.push_back(r.first);
        const DWORD which = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE) - WAIT_OBJECT_0;
        auto [h, unit_log] = running[which];
        running.erase(running.begin() + which);
        if (finish(h) != 0) {
            compile_failed = true;
            progress.line(tail(unit_log, 20));
        }
        ++finished;
        progress.fraction = 0.22f + 0.70f * static_cast<float>(finished) / static_cast<float>(sources.size());
        if (progress.cancel)
            for (auto &r : running) TerminateProcess(r.first, 1);
    }
    for (auto &r : running) finish(r.first);
    if (cancelled()) return false;
    if (compile_failed) return fail("the compiler reported errors (see logs\\build.log)");
    progress.line(std::to_string(sources.size()) + " files compiled");

    progress.status("Linking game.dll");
    const fs::path rsp = paths.game / "obj" / "link.rsp";
    {
        std::ofstream out(rsp);
        out << "-shared -static -o \"" << (paths.root / "game.dll").generic_string() << "\"\n";
        for (const auto &o : objects) out << '"' << o.generic_string() << "\"\n";
        out << '"' << (tools / "ps2" / "libCastlevaniaLoI.dll.a").generic_string() << "\"\n";
    }
    p = start(quote(cxx) + L" @" + quote(rsp), log);
    if (!p) return fail("cannot start the linker");
    if (finish(p) != 0) return fail("linking failed:\n" + tail(log, 15));

    std::ofstream stamp(paths.game / "built.txt");
    stamp << "runner=" << runner_stamp() << "\nsha1=" << config.get("sha1") << "\n";
    stamp.close();
    fs::remove_all(paths.game / "obj", ec);
    progress.fraction = 1.0f;
    progress.status("Ready to play");
    progress.done = true;
    progress.running = false;
    return true;
}

int extract_boot(const fs::path &config_file, const std::string &iso, const fs::path &out_dir) {
    GameConfig config;
    std::string boot, error;
    std::vector<std::uint8_t> elf;
    if (!GameConfig::load(config_file, config, error) || !check_disc({}, config, iso, boot, error) || !read_disc_file(boot, elf)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (!write_file(out_dir / boot, elf.data(), elf.size())) return 1;
    std::fprintf(stderr, "%s\n", (out_dir / boot).string().c_str());
    return 0;
}

int write_vu_recipe(const fs::path &elf_path, const fs::path &vu1_dir, const fs::path &out_path) {
    std::vector<std::uint8_t> elf;
    if (!read_file(elf_path, elf)) {
        std::fprintf(stderr, "cannot read %s\n", elf_path.string().c_str());
        return 1;
    }
    std::ofstream out(out_path);
    out << "# VU1 microcode images rebuilt from the boot executable: file vu:elf:size ... (hex)\n";
    for (const auto &e : fs::directory_iterator(vu1_dir)) {
        if (e.path().extension() != ".bin") continue;
        std::vector<std::uint8_t> image;
        read_file(e.path(), image);
        out << e.path().filename().string();
        std::vector<std::uint8_t> rebuilt(image.size(), 0);
        for (std::size_t i = 0; i < image.size();) {
            if (std::all_of(image.begin() + i, image.begin() + std::min(i + 8, image.size()), [](std::uint8_t b) { return b == 0; })) {
                i += 8;
                continue;
            }
            // Longest run of the ELF matching the image from here, in whole 64-bit instructions.
            std::size_t best_len = 0, best_at = 0;
            for (auto it = std::search(elf.begin(), elf.end(), image.begin() + i, image.begin() + i + 8); it != elf.end();
                 it = std::search(it + 1, elf.end(), image.begin() + i, image.begin() + i + 8)) {
                const std::size_t at = static_cast<std::size_t>(it - elf.begin());
                std::size_t len = 0;
                while (i + len < image.size() && at + len < elf.size() && image[i + len] == elf[at + len]) ++len;
                len -= len % 8;
                if (len > best_len) best_len = len, best_at = at;
            }
            if (!best_len) {
                std::fprintf(stderr, "%s: bytes at 0x%zX are not in the executable\n", e.path().filename().string().c_str(), i);
                return 1;
            }
            out << ' ' << std::hex << i << ':' << best_at << ':' << best_len << std::dec;
            std::memcpy(rebuilt.data() + i, elf.data() + best_at, best_len);
            i += best_len;
        }
        out << '\n';
        if (rebuilt != image) {
            std::fprintf(stderr, "%s: recipe does not rebuild the image\n", e.path().filename().string().c_str());
            return 1;
        }
        std::fprintf(stderr, "%s: ok\n", e.path().filename().string().c_str());
    }
    return 0;
}

Settings Settings::load(const Paths &paths) {
    Settings s;
    const auto keys = read_keys(paths.root / "settings.ini");
    const auto get = [&](const char *k, const std::string &d) {
        const auto it = keys.find(k);
        return it == keys.end() ? d : it->second;
    };
    s.iso = get("iso", "");
    s.fullscreen = get("fullscreen", "0") == "1";
    s.widescreen = get("widescreen", "0") == "1";
    s.scale = std::clamp(std::atoi(get("scale", "1").c_str()), 1, 3);
    return s;
}

void Settings::save(const Paths &paths) const {
    std::ofstream out(paths.root / "settings.ini");
    out << "iso=" << iso << "\nfullscreen=" << (fullscreen ? 1 : 0) << "\nwidescreen=" << (widescreen ? 1 : 0) << "\nscale=" << scale << "\n";
}

}
