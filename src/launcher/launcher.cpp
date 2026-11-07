// The window a player sees when starting the executable without arguments: choose the disc image,
// build the game code from it once, set a few options and play.
#include "launcher.hpp"

#include "builder.hpp"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "graphics_ui.hpp"

#include <windows.h>
#include <commdlg.h>

#include <cstdio>
#include <memory>
#include <thread>

namespace launcher {

namespace {

std::string utf8(const std::wstring &w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string choose_iso(SDL_Window *) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"PS2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Choose your Castlevania: Lament of Innocence disc image";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? utf8(file) : std::string{};
}

// Starts this executable again in game mode with the player's settings.
bool play(const Paths &paths, const GameConfig &config, const Settings &s) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::error_code ec;
    fs::create_directories(paths.saves, ec);
    fs::create_directories(paths.logs, ec);
    const auto q = [](const fs::path &p) { return L"\"" + p.wstring() + L"\""; };
    std::wstring cmd = q(exe) + L" " + q(paths.game / config.get("boot")) + L" --memcard " + q(paths.saves / "memcard1.ps2") +
                       L" --window";
    if (s.graphics.fullscreen) cmd += L" --fullscreen";
    if (s.graphics.widescreen) cmd += L" --widescreen";
    // Passed in 4:3 too, so switching to 16:9 in the F10 menu can narrow the 2D layer (unused in 4:3).
    if (!config.get("widescreen_2d_vu1").empty())
        cmd += L" --widescreen-2d-vu1 " + fs::path(config.get("widescreen_2d_vu1")).wstring();
    // Quick save / load (F5 / F6): a developer build inside the repository uses its .savestates
    // folder, so states outlive clean builds; any other copy keeps them beside the executable.
    fs::path states = paths.root / "savestates";
    for (fs::path dir = paths.root; dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
        if (fs::is_directory(dir / ".savestates", ec)) {
            states = dir / ".savestates";
            break;
        }
    cmd += L" --states " + q(states);
    cmd += L" --settings " + q(paths.root / "settings.ini");
    SetEnvironmentVariableW(L"PS2_SCALE", std::to_wstring(s.graphics.scale).c_str());
    // The game writes its log beside the others; a launched game has no console.
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = CreateFileW((paths.logs / "game.log").wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (log != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = log;
    }
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, paths.root.wstring().c_str(), &si, &pi);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

}  // namespace

int run() {
    const Paths paths = Paths::from_exe();
    GameConfig config;
    std::string config_error;
    const bool have_config = GameConfig::load(paths.compiler / "config" / "game.txt", config, config_error);
    Settings settings = Settings::load(paths);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        MessageBoxA(nullptr, SDL_GetError(), "Launcher", MB_ICONERROR);
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Window *window = SDL_CreateWindow(have_config ? (config.get("title") + " - Launcher").c_str() : "Launcher", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 760, 520, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        MessageBoxA(nullptr, SDL_GetError(), "Launcher", MB_ICONERROR);
        return 1;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.Colors[ImGuiCol_Button] = ImVec4(0.45f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.12f, 0.14f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.15f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.65f, 0.12f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.85f, 0.25f, 0.28f, 1.0f);
    ImFontConfig font;
    font.SizePixels = 18.0f;
    io.Fonts->AddFontDefault(&font);
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    Progress progress;
    std::unique_ptr<std::thread> worker;
    std::string disc_message;
    bool disc_ok = false;
    bool built = have_config && is_built(paths, config);
    // The game plays from the files the build copied off the disc; the image is only for building.
    std::string data_message;
    bool data_ok = have_config && check_data(paths, config, data_message);
    if (have_config && !settings.iso.empty()) {
        std::string boot;
        disc_ok = check_disc(paths, config, settings.iso, boot, disc_message);
    }
    bool quit = false, launched = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
        }
        if (worker && !progress.running) {
            worker->join();
            worker.reset();
            built = is_built(paths, config);
            data_ok = check_data(paths, config, data_message);
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        if (!have_config) {
            ImGui::TextWrapped("This copy of the launcher is incomplete: %s\n\n"
                               "Run CastlevaniaLoI.exe from a complete folder: the release zip extracted whole, "
                               "or, for a developer build, output\\debug\\CastlevaniaLoI (made by build.bat).",
                               config_error.c_str());
        } else {
            ImGui::SetWindowFontScale(1.5f);
            ImGui::TextUnformatted(config.get("title").c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextDisabled("Native PC version built from your own %s disc", config.get("serial").c_str());
            ImGui::Separator();

            const bool busy = static_cast<bool>(worker);
            ImGui::TextUnformatted("Disc image");
            ImGui::BeginDisabled(busy);
            if (ImGui::Button(settings.iso.empty() ? "Choose disc image..." : "Change...")) {
                const std::string picked = choose_iso(window);
                if (!picked.empty()) {
                    std::string boot;
                    settings.iso = picked;
                    disc_ok = check_disc(paths, config, picked, boot, disc_message);
                    settings.save(paths);
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", settings.iso.empty() ? "none chosen" : settings.iso.c_str());
            if (data_ok) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.85f, 0.5f, 1));
                ImGui::TextWrapped("Game files copied from the disc: the disc image is no longer needed.");
                ImGui::PopStyleColor();
            } else if (!settings.iso.empty()) {
                if (disc_ok)
                    ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.5f, 1), "Disc verified: %s", config.get("serial").c_str());
                else
                    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1), "%s", disc_message.c_str());
            }
            ImGui::Separator();

            if (busy || progress.failed) {
                ImGui::TextUnformatted(progress.status().c_str());
                ImGui::ProgressBar(progress.fraction.load(), ImVec2(-1, 0));
                const auto lines = progress.lines();
                ImGui::BeginChild("##log", ImVec2(-1, ImGui::GetContentRegionAvail().y - 50), ImGuiChildFlags_Borders);
                for (const auto &l : lines) ImGui::TextUnformatted(l.c_str());
                if (busy) ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();
                if (busy && ImGui::Button("Cancel")) progress.cancel = true;
                if (!busy && ImGui::Button("Back")) progress.failed = false;
            } else if (!built) {
                ImGui::TextWrapped("The first time, the launcher builds the game's code from your disc. It takes a few minutes and happens only once.");
                ImGui::BeginDisabled(!disc_ok && !data_ok);
                if (ImGui::Button("Build the game", ImVec2(220, 44))) {
                    progress.cancel = false;
                    progress.running = true;
                    worker = std::make_unique<std::thread>([&] { build(paths, config, settings.iso, progress); });
                }
                ImGui::EndDisabled();
            } else {
                // The same options as the in-game F10 menu (graphics_ui.cpp).
                ImGui::PushItemWidth(200);
                ps2ui::graphics_options(settings.graphics, false);
                ImGui::PopItemWidth();
                ImGui::Spacing();
                ImGui::BeginDisabled(!data_ok);
                if (ImGui::Button("Play", ImVec2(220, 50))) {
                    settings.save(paths);
                    if (play(paths, config, settings)) {
                        launched = true;
                        quit = true;
                    } else {
                        disc_message = "Could not start the game.";
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(!disc_ok && !data_ok);
                if (ImGui::Button("Rebuild")) built = false;
                ImGui::EndDisabled();
            }
        }
        ImGui::End();
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 12, 10, 14, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    if (worker) {
        progress.cancel = true;
        worker->join();
    }
    settings.save(paths);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    (void)launched;
    return 0;
}

int build_cli(const std::string &iso) {
    const Paths paths = Paths::from_exe();
    GameConfig config;
    std::string error;
    if (!GameConfig::load(paths.compiler / "config" / "game.txt", config, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    Progress progress;
    std::thread t([&] { build(paths, config, iso, progress); });
    std::size_t shown = 0;
    while (progress.running || !(progress.done || progress.failed)) {
        const auto lines = progress.lines();
        for (; shown < lines.size(); ++shown) std::fprintf(stderr, "%s\n", lines[shown].c_str());
        Sleep(200);
    }
    t.join();
    const auto lines = progress.lines();
    for (; shown < lines.size(); ++shown) std::fprintf(stderr, "%s\n", lines[shown].c_str());
    return progress.done ? 0 : 1;
}

}
