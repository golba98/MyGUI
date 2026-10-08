#include "gui/Button.hpp"
#include "gui/Renderer.hpp"
#include "gui/UIContext.hpp"
#include "gui/Window.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>

namespace {
class HoverPanel : public gui::Panel {
public:
    void draw(gui::Renderer& renderer) const override {
        if (visible()) renderer.drawRect(bounds(), hovered()
            ? gui::Color{1.0f, 0.55f, 0.25f, 0.6f} : backgroundColor());
    }
};

// Custom containers can arrange absolute artwork relative to their content area.
class Gallery : public gui::Container {
public:
    explicit Gallery(const std::shared_ptr<gui::Font>& font) {
        setBackgroundColor({0.10f, 0.12f, 0.16f, 1});
        setPadding({16, 16, 16, 16});
        setMinimumSize({160, 120}); setPreferredSize({480, 240}); setFlex(1);
        base_ = &emplace<gui::Panel>();
        base_->setBackgroundColor({0.15f, 0.45f, 0.40f, 1});
        overlay_ = &emplace<HoverPanel>();
        overlay_->setBackgroundColor({0.90f, 0.25f, 0.35f, 0.55f});
        caption_ = &emplace<gui::Label>("Clipped layers · hover the overlay", font);
    }
    gui::Panel& overlay() noexcept { return *overlay_; }
    void arrange() override {
        gui::Container::arrange();
        const auto b = contentBounds();
        base_->setBounds({b.x + 12, b.y + 60, std::max(100.0f, b.width - 80), std::max(80.0f, b.height - 80)});
        overlay_->setBounds({b.x + 100, b.y + 100, 340, 180});
        caption_->setBounds({b.x, b.y, b.width, 32});
    }
private:
    gui::Panel* base_{};
    HoverPanel* overlay_{};
    gui::Label* caption_{};
};

std::filesystem::path executableDirectory(const char* argv0) {
    std::error_code error;
#ifdef __linux__
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return executable.parent_path();
#endif
    return std::filesystem::absolute(argv0).parent_path();
}
}

int main(int argc, char** argv) {
    try {
        bool logEvents = false;
        int maxFrames = 0, width = 1280, height = 720;
        std::filesystem::path fontPath = executableDirectory(argv[0]) / "assets/fonts/NotoSans.ttf";
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "MyGUI [--log-events] [--font PATH] [--frames N] [--width N] [--height N]\n";
                return 0;
            }
            if (option == "--log-events") { logEvents = true; continue; }
            if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + option);
            const std::string value = argv[++i];
            if (option == "--font") fontPath = value;
            else if (option == "--frames" || option == "--width" || option == "--height") {
                std::size_t consumed = 0;
                const auto number = std::stoi(value, &consumed);
                if (consumed != value.size() || number <= 0) throw std::invalid_argument("Expected a positive integer for " + option);
                if (option == "--frames") maxFrames = number;
                else if (option == "--width") width = number;
                else height = number;
            }
            else throw std::invalid_argument("Unknown option: " + option);
        }
        const auto font = gui::Font::load(fontPath.string());
        gui::Window window{width, height, "MyGUI — interactive widgets"};
        window.setSwapInterval(1);
        window.setEventDelivery(gui::EventDelivery::Queue);
        // GPU resources are released before the window destroys its context.
        gui::Renderer renderer{window.glProcLoader()};
        gui::UIContext ui;
        auto& root = ui.root();
        root.setLayout(gui::Layout::Horizontal); root.setPadding({24, 24, 24, 24}); root.setSpacing(20);
        root.setBackgroundColor({0.07f, 0.08f, 0.11f, 1});

        auto& sidebar = root.emplace<gui::Container>();
        sidebar.setLayout(gui::Layout::Vertical); sidebar.setPadding({20, 20, 20, 20}); sidebar.setSpacing(14);
        sidebar.setPreferredSize({240, 0}); sidebar.setMinimumSize({180, 0});
        sidebar.setBackgroundColor({0.13f, 0.16f, 0.21f, 1});
        auto& brand = sidebar.emplace<gui::Label>("MyGUI", font); brand.setFontSize(26);
        auto& subtitle = sidebar.emplace<gui::Label>("C++20 · OpenGL 3.3", font); subtitle.setFontSize(13);
        auto& toggle = sidebar.emplace<gui::Button>("Toggle overlay", font);
        auto& counter = sidebar.emplace<gui::Button>("Count a click", font);
        auto& disabled = sidebar.emplace<gui::Button>("Disabled button", font); disabled.setEnabled(false);
        auto& spacer = sidebar.emplace<gui::Panel>(); spacer.setFlex(1); spacer.setBackgroundColor(sidebar.backgroundColor());
        auto& instructions = sidebar.emplace<gui::Label>("Tab / Shift+Tab: focus\nEnter / Space: activate\nV: toggle overlay", font); instructions.setFontSize(12);

        auto& content = root.emplace<gui::Container>();
        content.setLayout(gui::Layout::Vertical); content.setPadding({24, 24, 24, 24}); content.setSpacing(16);
        content.setMinimumSize({220, 0}); content.setFlex(1);
        content.setBackgroundColor({0.11f, 0.13f, 0.18f, 1});
        auto& heading = content.emplace<gui::Label>("A foundation for interactive UI", font); heading.setFontSize(28);
        auto& description = content.emplace<gui::Label>("Owned widgets, automatic layout, keyboard focus, and HiDPI text.", font); description.setFontSize(14);
        auto& gallery = content.emplace<Gallery>(font);
        auto& status = content.emplace<gui::Label>("Clicks: 0 · Overlay visible", font); status.setFontSize(15);
        int clicks = 0;
        const auto updateStatus = [&] {
            status.setText("Clicks: " + std::to_string(clicks) + (gallery.overlay().visible() ? " · Overlay visible" : " · Overlay hidden"));
        };
        const auto toggleOverlay = [&] { gallery.overlay().setVisible(!gallery.overlay().visible()); updateStatus(); };
        toggle.setOnClick(toggleOverlay);
        counter.setOnClick([&] { ++clicks; updateStatus(); });

        int frames = 0;
        while (!window.shouldClose() && (!maxFrames || frames < maxFrames)) {
            window.pollEvents();
            ui.layout(window.viewport());
            while (auto event = window.nextEvent()) {
                if (logEvents) std::cout << "Event " << static_cast<int>(event->type()) << '\n';
                if (!ui.handleEvent(*event)) {
                    if (const auto* key = event->getIf<gui::KeyEvent>(); key && key->key == gui::Key::V && key->action == gui::KeyAction::Press)
                        toggleOverlay();
                }
            }
            if (window.shouldClose()) break;
            window.makeContextCurrent();
            renderer.beginFrame(window.viewport()); renderer.clear({0.07f, 0.08f, 0.11f, 1});
            ui.layout(window.viewport()); ui.draw(renderer); renderer.endFrame();
            window.swapBuffers(); ++frames;
        }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "Error: " << error.what() << '\n'; return 1; }
}
