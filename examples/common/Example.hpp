#pragma once

// Shared setup for the examples: bundled-font lookup, --frames for smoke
// tests, and an event loop that sleeps until input arrives and redraws only
// when the UI changed.
#include "gui/Font.hpp"
#include "gui/Renderer.hpp"
#include "gui/UIContext.hpp"
#include "gui/Window.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace example {
inline std::filesystem::path executableDirectory(const char* argv0) {
    std::error_code error;
#ifdef __linux__
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return executable.parent_path();
#endif
    return std::filesystem::absolute(argv0).parent_path();
}

// The build copies assets/ next to every example.
inline std::shared_ptr<gui::Font> loadFont(const char* argv0) {
    return gui::Font::load((executableDirectory(argv0) / "assets/fonts/NotoSans.ttf").string());
}

// Parses the only option the small examples accept: --frames N.
inline int frameLimit(int argc, char** argv) {
    if (argc == 1) return 0;
    if (argc == 3 && std::string{argv[1]} == "--frames") {
        std::size_t consumed = 0;
        const int frames = std::stoi(argv[2], &consumed);
        if (consumed == std::string{argv[2]}.size() && frames > 0) return frames;
    }
    throw std::invalid_argument("Usage: " + std::string{argv[0]} + " [--frames N]");
}

// With maxFrames, polls and draws exactly that many frames, then returns.
// Otherwise waits for events and draws only when ui.needsRedraw().
// Events the UI does not handle go to onEvent.
inline void run(gui::Window& window, gui::Renderer& renderer, gui::UIContext& ui, int maxFrames = 0,
                const std::function<void(const gui::Event&)>& onEvent = {}) {
    window.setEventDelivery(gui::EventDelivery::Queue);
    ui.setClipboard(&window.clipboard());
    for (int frames = 0; !window.shouldClose();) {
        ui.layout(window.viewport());
        if (maxFrames || ui.needsRedraw()) {
            window.makeContextCurrent();
            renderer.beginFrame(window.viewport());
            renderer.clear(ui.theme().background);
            ui.draw(renderer);
            renderer.endFrame();
            window.swapBuffers();
            if (maxFrames && ++frames >= maxFrames) break;
        }
        if (maxFrames) window.pollEvents();
        else window.waitEvents();
        ui.layout(window.viewport());
        while (auto event = window.nextEvent()) {
            if (!ui.handleEvent(*event) && onEvent) onEvent(*event);
        }
    }
}
} // namespace example
