// TempleFrog: the window shown while the game is built (see build_screen.h).
#include "build_screen.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>

#if defined(GBAGAME_RECOMP_UI)
#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#endif
#if __has_include("launcher_i18n.h")
#include "launcher_i18n.h"
#define TF_SCREEN_I18N 1
#endif

namespace {

const char* tx(const char* english) {
#if defined(TF_SCREEN_I18N)
    return launcher_i18n_text(english);
#else
    return english;
#endif
}

struct Shared {
    std::mutex mutex;
    float fraction = 0.0f;
    std::string text;
    std::string error;
    std::atomic<bool> finished{false};
    bool ok = false;
};

// Progress to the log only (no display, or no launcher UI in this build).
bool run_headless(const std::function<bool(const TfProgress&, std::string*)>& job,
                  std::string* error) {
    int last = -1;
    const bool ok = job([&](float f, const std::string& text) {
        const int pct = static_cast<int>(f * 100.0f);
        if (pct / 10 != last / 10 || pct == 100) {
            std::fprintf(stderr, "[templefrog] building the game: %d%% %s\n", pct, text.c_str());
            last = pct;
        }
    }, error);
    return ok;
}

}  // namespace

bool tf_run_build_screen(const std::function<bool(const TfProgress&, std::string*)>& job,
                         const std::string& log_path, std::string* error) {
#if !defined(GBAGAME_RECOMP_UI)
    (void)log_path;
    return run_headless(job, error);
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return run_headless(job, error);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_Window* window = SDL_CreateWindow("TempleFrog", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 720, 300,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext gl = window ? SDL_GL_CreateContext(window) : nullptr;
    if (!window || !gl) {
        if (window) SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return run_headless(job, error);
    }
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImFont* body = nullptr;
    ImFont* title = nullptr;
    {
        // TempleFrog's menu fonts, next to the program (the working directory
        // can be elsewhere: a shortcut, a launcher, a developer's checkout).
        std::string fonts = "assets/fonts/";
        if (char* base = SDL_GetBasePath()) {
            fonts = std::string(base) + "assets/fonts/";
            SDL_free(base);
        }
        const std::string body_ttf = fonts + "ui_body.ttf", title_ttf = fonts + "ui_title.ttf";
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::u8path(body_ttf), ec))
            body = io.Fonts->AddFontFromFileTTF(body_ttf.c_str(), 20.0f, nullptr,
                                                io.Fonts->GetGlyphRangesDefault());
        if (std::filesystem::exists(std::filesystem::u8path(title_ttf), ec))
            title = io.Fonts->AddFontFromFileTTF(title_ttf.c_str(), 34.0f, nullptr,
                                                 io.Fonts->GetGlyphRangesDefault());
    }
    // TempleFrog's "temple at dusk" colours (src/main.cpp ui_theme).
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 6.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0x20 / 255.f, 0x10 / 255.f, 0x10 / 255.f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0xF0 / 255.f, 0xF0 / 255.f, 0xC0 / 255.f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0x40 / 255.f, 0x20 / 255.f, 0x10 / 255.f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0xA0 / 255.f, 0xE0 / 255.f, 0x10 / 255.f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0x40 / 255.f, 0x20 / 255.f, 0x10 / 255.f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0x50 / 255.f, 0x30 / 255.f, 0x18 / 255.f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0x60 / 255.f, 0xB0 / 255.f, 0x00 / 255.f, 1.0f);
    const ImVec4 muted(0xC8 / 255.f, 0xB0 / 255.f, 0x88 / 255.f, 1.0f);
    const ImVec4 warn(0xE0 / 255.f, 0xA0 / 255.f, 0x80 / 255.f, 1.0f);
    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");

    Shared shared;
    std::thread worker([&]() {
        std::string err;
        const bool ok = job([&](float f, const std::string& text) {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.fraction = f;
            shared.text = text;
        }, &err);
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.ok = ok;
            shared.error = err;
        }
        shared.finished = true;
    });

    bool closed = false;
    const auto started = std::chrono::steady_clock::now();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT ||
                (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE))
                closed = true;
            if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_KEYDOWN) {
                if (shared.finished && !shared.ok) closed = true;
            }
        }
        if (shared.finished && shared.ok && !closed) break;
        if (closed && shared.finished) break;
        if (closed) SDL_HideWindow(window);  // the compiler finishes in the background

        float fraction;
        std::string text, err;
        bool failed;
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            fraction = shared.fraction;
            text = shared.text;
            err = shared.error;
            failed = shared.finished && !shared.ok;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("build", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorPos(ImVec2(28, 24));
        if (title) ImGui::PushFont(title);
        ImGui::TextUnformatted(failed ? tx("TempleFrog couldn't build the game")
                                      : tx("Building TempleFrog"));
        if (title) ImGui::PopFont();
        if (body) ImGui::PushFont(body);
        ImGui::SetCursorPosX(28);
        ImGui::PushTextWrapPos(io.DisplaySize.x - 28);
        if (!failed) {
            ImGui::TextColored(muted, "%s",
                tx("TempleFrog is making the game from your own ROM and BIOS. This happens "
                   "once and takes a minute or two."));
            ImGui::Spacing();
            ImGui::SetCursorPosX(28);
            ImGui::ProgressBar(fraction, ImVec2(io.DisplaySize.x - 56, 26), "");
            ImGui::SetCursorPosX(28);
            const long secs = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started).count());
            ImGui::TextColored(muted, "%s  (%ld s)", text.c_str(), secs);
        } else {
            ImGui::TextColored(warn, "%s", err.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX(28);
            ImGui::TextColored(muted, "%s %s", tx("Details are in"), log_path.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX(28);
            if (ImGui::Button(tx("Close"), ImVec2(140, 36))) closed = true;
        }
        ImGui::PopTextWrapPos();
        if (body) ImGui::PopFont();
        ImGui::End();
        ImGui::Render();
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0x20 / 255.f, 0x10 / 255.f, 0x10 / 255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
    worker.join();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(previous);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    // Closing the window stops TempleFrog, even when the build got done.
    if (error) *error = closed ? std::string() : shared.error;
    return shared.ok && !closed;
#endif
}
