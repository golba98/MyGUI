#include "gui/Button.hpp"
#include "gui/Checkbox.hpp"
#include "gui/ImageView.hpp"
#include "gui/ProgressBar.hpp"
#include "gui/ScrollView.hpp"
#include "gui/Slider.hpp"
#include "gui/TextField.hpp"
#include "gui/Renderer.hpp"
#include "gui/UIContext.hpp"
#include "gui/Window.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
const char* actionName(gui::KeyAction action) {
    switch (action) {
        case gui::KeyAction::Press: return "pressed";
        case gui::KeyAction::Release: return "released";
        case gui::KeyAction::Repeat: return "repeated";
    }
    return "unknown action";
}

void describeModifiers(std::ostream& out, const gui::Modifiers& mods) {
    out << " modifiers=[" << (mods.shift ? "Shift " : "")
        << (mods.control ? "Ctrl " : "") << (mods.alt ? "Alt " : "")
        << (mods.super ? "Super " : "") << ']';
}

std::string describeEvent(const gui::Event& event) {
    std::ostringstream out;
    if (const auto* key = event.getIf<gui::KeyEvent>()) {
        out << "Key " << gui::toString(key->key) << ' ' << actionName(key->action)
            << " scancode=" << key->scancode;
        describeModifiers(out, key->mods);
    } else if (const auto* move = event.getIf<gui::MouseMoveEvent>()) {
        out << "Mouse moved: (" << move->x << ", " << move->y << ')';
    } else if (const auto* button = event.getIf<gui::MouseButtonEvent>()) {
        out << "Mouse " << gui::toString(button->button)
            << (button->action == gui::ButtonAction::Press ? " pressed" : " released")
            << " at (" << button->x << ", " << button->y << ')';
        describeModifiers(out, button->mods);
    } else if (const auto* scroll = event.getIf<gui::MouseScrollEvent>()) {
        out << "Scroll: (" << scroll->xOffset << ", " << scroll->yOffset
            << ") at (" << scroll->x << ", " << scroll->y << ')';
    } else if (const auto* resize = event.getIf<gui::WindowResizeEvent>()) {
        out << (resize->source == gui::ResizeSource::Window ? "Window" : "Framebuffer")
            << " resized: logical=" << resize->logicalWidth << 'x' << resize->logicalHeight
            << " framebuffer=" << resize->width << 'x' << resize->height;
    } else if (const auto* text = event.getIf<gui::TextInputEvent>()) {
        out << "Text input: U+" << std::hex << std::uppercase << static_cast<unsigned int>(text->codepoint);
    } else if (const auto* focus = event.getIf<gui::WindowFocusEvent>()) {
        out << "Window focus " << (focus->focused ? "gained" : "lost");
    } else if (const auto* enter = event.getIf<gui::CursorEnterEvent>()) {
        out << "Cursor " << (enter->entered ? "entered" : "left");
    } else if (event.getIf<gui::WindowCloseEvent>()) {
        out << "Window closing";
    }
    return out.str();
}

class HoverPanel : public gui::Panel {
public:
    void draw(gui::Renderer& renderer) const override {
        if (!visible()) return;
        renderer.drawRoundedRect(bounds(), 18, hovered() ? gui::Color{1.0f, 0.55f, 0.25f, 0.6f} : backgroundColor());
        renderer.drawBorder(bounds(), 2, {1, 1, 1, 0.35f}, 18);
    }
};

// Generated in code so the demo needs no image asset.
std::shared_ptr<gui::Image> gradientImage() {
    constexpr int size = 96;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(size * size * 4);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const bool check = (x / 12 + y / 12) % 2 == 0;
        pixels.insert(pixels.end(), {static_cast<std::uint8_t>(x * 255 / size), static_cast<std::uint8_t>(y * 255 / size),
                                     static_cast<std::uint8_t>(check ? 230 : 160), 255});
    }
    return gui::Image::fromPixels(size, size, std::move(pixels));
}

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
        image_ = &emplace<gui::ImageView>(gradientImage());
    }
    gui::Panel& overlay() noexcept { return *overlay_; }
    void arrange() override {
        gui::Container::arrange();
        const auto b = contentBounds();
        base_->setBounds({b.x + 12, b.y + 60, std::max(100.0f, b.width - 80), std::max(80.0f, b.height - 80)});
        overlay_->setBounds({b.x + 100, b.y + 100, 340, 180});
        caption_->setBounds({b.x, b.y, b.width, 32});
        image_->setBounds({b.x + b.width - 108, b.y + 40, 96, 96});
    }
private:
    gui::Panel* base_{};
    HoverPanel* overlay_{};
    gui::Label* caption_{};
    gui::ImageView* image_{};
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

        auto& sidebarView = root.emplace<gui::ScrollView>();
        sidebarView.setPreferredSize({240, 0}); sidebarView.setMinimumSize({180, 0});
        sidebarView.setBackgroundColor({0.13f, 0.16f, 0.21f, 1});
        auto& sidebar = sidebarView.content();
        sidebar.setPadding({20, 20, 20, 20}); sidebar.setSpacing(14);
        auto& brand = sidebar.emplace<gui::Label>("MyGUI", font); brand.setFontSize(26);
        auto& subtitle = sidebar.emplace<gui::Label>("C++20 · OpenGL 3.3", font); subtitle.setFontSize(13);
        auto& toggle = sidebar.emplace<gui::Button>("Toggle overlay", font);
        auto& counter = sidebar.emplace<gui::Button>("Count a click", font);
        auto& disabled = sidebar.emplace<gui::Button>("Disabled button", font); disabled.setEnabled(false);
        auto& overlayBox = sidebar.emplace<gui::Checkbox>("Show overlay", font, true);
        auto& warmTheme = sidebar.emplace<gui::Switch>("Warm accent", font);
        auto& listHeading = sidebar.emplace<gui::Label>("Scrollable options", font); listHeading.setFontSize(13);
        for (int i = 1; i <= 12; ++i) sidebar.emplace<gui::Checkbox>("Option " + std::to_string(i), font, i % 3 == 0);
        auto& spacer = sidebar.emplace<gui::Panel>(); spacer.setFlex(1); spacer.setBackgroundColor({0, 0, 0, 0});
        auto& instructions = sidebar.emplace<gui::Label>("Tab / Shift+Tab: focus\nEnter / Space: activate\nArrows: adjust slider\nWheel / PageUp / PageDown: scroll\nV: toggle overlay", font); instructions.setFontSize(12);

        auto& content = root.emplace<gui::Container>();
        content.setLayout(gui::Layout::Vertical); content.setPadding({24, 24, 24, 24}); content.setSpacing(16);
        content.setMinimumSize({220, 0}); content.setFlex(1);
        content.setBackgroundColor({0.11f, 0.13f, 0.18f, 1});
        auto& heading = content.emplace<gui::Label>("A foundation for interactive UI", font); heading.setFontSize(28);
        auto& description = content.emplace<gui::Label>("Owned widgets, automatic layout, keyboard focus, and HiDPI text.", font); description.setFontSize(14);
        auto& gallery = content.emplace<Gallery>(font);
        auto& sliderRow = content.emplace<gui::Container>();
        sliderRow.setLayout(gui::Layout::Horizontal); sliderRow.setSpacing(16); sliderRow.setBackgroundColor({0, 0, 0, 0});
        auto& sliderLabel = sliderRow.emplace<gui::Label>("Progress: 40%", font); sliderLabel.setPreferredSize({130, 0});
        sliderLabel.setAlignment(gui::HorizontalAlign::Left, gui::VerticalAlign::Center);
        auto& slider = sliderRow.emplace<gui::Slider>(0.0f, 100.0f, 40.0f); slider.setStep(1); slider.setFlex(1);
        auto& progress = content.emplace<gui::ProgressBar>(0.4f);
        auto& noteRow = content.emplace<gui::Container>();
        noteRow.setLayout(gui::Layout::Horizontal); noteRow.setSpacing(12); noteRow.setBackgroundColor({0, 0, 0, 0});
        auto& note = noteRow.emplace<gui::TextField>(font); note.setPlaceholder("Type a note and press Enter"); note.setFlex(1);
        auto& submit = noteRow.emplace<gui::Button>("Submit", font);
        auto& status = content.emplace<gui::Label>("Clicks: 0 · Overlay visible", font); status.setFontSize(15);
        auto& inputStatus = content.emplace<gui::Label>("", font); inputStatus.setFontSize(12);
        auto& lastTransition = content.emplace<gui::Label>("Last input: none", font); lastTransition.setFontSize(12);
        int clicks = 0;
        std::string lastNote;
        const auto updateStatus = [&] {
            status.setText("Clicks: " + std::to_string(clicks) + (gallery.overlay().visible() ? " · Overlay visible" : " · Overlay hidden")
                           + (lastNote.empty() ? "" : " · Note: " + lastNote));
        };
        const auto submitNote = [&] { lastNote = note.text(); note.setText(""); updateStatus(); };
        note.setOnSubmit([&](const std::string&) { submitNote(); });
        submit.setOnClick(submitNote);
        const auto setOverlay = [&](bool visible) {
            gallery.overlay().setVisible(visible); overlayBox.setChecked(visible); updateStatus();
        };
        const auto toggleOverlay = [&] { setOverlay(!gallery.overlay().visible()); };
        toggle.setOnClick(toggleOverlay);
        overlayBox.setOnChange(setOverlay);
        warmTheme.setOnChange([&](bool warm) {
            gui::Theme theme;
            if (warm) {
                theme.accent = {0.55f, 0.27f, 0.12f, 1}; theme.accentHovered = {0.70f, 0.36f, 0.16f, 1};
                theme.accentPressed = {0.42f, 0.20f, 0.08f, 1}; theme.fill = {0.98f, 0.62f, 0.30f, 1};
                theme.focus = {1.0f, 0.82f, 0.55f, 1};
            }
            ui.setTheme(theme);
        });
        slider.setOnChange([&](float value) {
            progress.setValue(value / 100);
            sliderLabel.setText("Progress: " + std::to_string(static_cast<int>(value)) + "%");
        });
        counter.setOnClick([&] { ++clicks; updateStatus(); });

        int frames = 0;
        while (!window.shouldClose() && (!maxFrames || frames < maxFrames)) {
            window.pollEvents();
            ui.layout(window.viewport());
            while (auto event = window.nextEvent()) {
                if (logEvents) std::cout << describeEvent(*event) << '\n';
                if (event->getIf<gui::KeyEvent>() || event->getIf<gui::MouseButtonEvent>()
                    || event->getIf<gui::TextInputEvent>())
                    lastTransition.setText(describeEvent(*event));
                if (!ui.handleEvent(*event)) {
                    if (const auto* key = event->getIf<gui::KeyEvent>(); key && key->key == gui::Key::V && key->action == gui::KeyAction::Press)
                        toggleOverlay();
                }
            }
            const auto& input = window.input();
            std::ostringstream diagnostics;
            diagnostics << std::fixed << std::setprecision(2)
                << "Mouse: (" << input.mouseX() << ", " << input.mouseY() << ")  Delta: ("
                << input.mouseDeltaX() << ", " << input.mouseDeltaY() << ")  Scroll: ("
                << input.scrollX() << ", " << input.scrollY() << ")\n"
                << "Left: down=" << input.isMouseButtonDown(gui::MouseButton::Left)
                << " pressed=" << input.isMouseButtonPressed(gui::MouseButton::Left)
                << " released=" << input.isMouseButtonReleased(gui::MouseButton::Left)
                << "    V: down=" << input.isKeyDown(gui::Key::V)
                << " pressed=" << input.isKeyPressed(gui::Key::V)
                << " released=" << input.isKeyReleased(gui::Key::V);
            const auto diagnosticText = diagnostics.str();
            if (inputStatus.text() != diagnosticText) inputStatus.setText(diagnosticText);
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
