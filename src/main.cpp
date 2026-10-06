#include "gui/Panel.hpp"
#include "gui/Renderer.hpp"
#include "gui/Window.hpp"

#include <exception>
#include <iostream>

namespace {

void printModifiers(const gui::Modifiers& mods) {
    if (mods.shift) std::cout << " +Shift";
    if (mods.control) std::cout << " +Ctrl";
    if (mods.alt) std::cout << " +Alt";
    if (mods.super) std::cout << " +Super";
}

void printKey(const char* label, const gui::Event& event) {
    const auto* key = event.getIf<gui::KeyEvent>();

    std::cout << label << ": " << gui::toString(key->key)
              << " (scancode " << key->scancode << ')';
    printModifiers(key->mods);
    std::cout << '\n';
}

void printMouseButton(const char* label, const gui::Event& event) {
    const auto* button = event.getIf<gui::MouseButtonEvent>();

    std::cout << label << ": " << gui::toString(button->button)
              << " at " << button->x << ", " << button->y;
    printModifiers(button->mods);
    std::cout << '\n';
}

void printEvent(const gui::Event& event) {
    switch (event.type()) {
        case gui::EventType::KeyPressed:
            printKey("Key pressed", event);
            break;
        case gui::EventType::KeyReleased:
            printKey("Key released", event);
            break;
        case gui::EventType::KeyRepeated:
            printKey("Key repeated", event);
            break;
        case gui::EventType::MouseMoved: {
            const auto* move = event.getIf<gui::MouseMoveEvent>();
            std::cout << "Mouse moved: " << move->x << ", " << move->y << '\n';
            break;
        }
        case gui::EventType::MouseButtonPressed:
            printMouseButton("Mouse button pressed", event);
            break;
        case gui::EventType::MouseButtonReleased:
            printMouseButton("Mouse button released", event);
            break;
        case gui::EventType::MouseScrolled: {
            const auto* scroll = event.getIf<gui::MouseScrollEvent>();
            std::cout << "Scroll: " << scroll->xOffset << ", " << scroll->yOffset << '\n';
            break;
        }
        case gui::EventType::WindowResized: {
            const auto* resize = event.getIf<gui::WindowResizeEvent>();
            std::cout << "Window resized: logical " << resize->logicalWidth << ", "
                      << resize->logicalHeight << "; framebuffer " << resize->width
                      << ", " << resize->height << '\n';
            break;
        }
        case gui::EventType::WindowClosed:
            std::cout << "Window closed\n";
            break;
        case gui::EventType::TextInput: {
            const auto* text = event.getIf<gui::TextInputEvent>();
            std::cout << "Text input: U+" << std::hex
                      << static_cast<unsigned int>(text->codepoint) << std::dec << '\n';
            break;
        }
    }
}

} // namespace

int main() {
    try {
        gui::Window window{1280, 720, "MyGUI"};

        // Declared after window so it is destroyed while the context is current.
        gui::Renderer renderer{window.glProcLoader()};

        // Positions are logical units; the renderer scales them for HiDPI.
        gui::Panel frame;
        frame.setBackgroundColor({0.14f, 0.15f, 0.17f, 1.0f});

        gui::Panel sidebar;
        sidebar.setBackgroundColor({0.24f, 0.26f, 0.30f, 1.0f});

        gui::Panel content;
        content.setBackgroundColor({0.20f, 0.22f, 0.25f, 1.0f});

        // Overlaps the sidebar and content. Press V to toggle it.
        gui::Panel overlay{{200.0f, 120.0f, 320.0f, 180.0f}};
        overlay.setBackgroundColor({0.90f, 0.25f, 0.35f, 0.5f});

        while (!window.shouldClose()) {
            window.pollEvents();
            while (auto event = window.nextEvent()) {
                printEvent(*event);
            }
            if (window.shouldClose()) {
                break;
            }
            const gui::Viewport viewport = window.viewport();
            const float width = static_cast<float>(viewport.logicalWidth);
            const float height = static_cast<float>(viewport.logicalHeight);

            if (window.input().isKeyPressed(gui::Key::V)) {
                overlay.setVisible(!overlay.visible());
            }

            frame.setBounds({20.0f, 20.0f, width - 40.0f, height - 40.0f});
            sidebar.setBounds({40.0f, 40.0f, 220.0f, height - 80.0f});
            content.setBounds({280.0f, 40.0f, width - 320.0f, height - 80.0f});

            const auto mouse = window.input().mousePosition();
            const auto& bounds = overlay.bounds();
            const bool hovered = overlay.visible() && mouse.x >= bounds.x
                && mouse.y >= bounds.y && mouse.x < bounds.x + bounds.width
                && mouse.y < bounds.y + bounds.height;
            overlay.setBackgroundColor(hovered
                ? gui::Color{1.0f, 0.55f, 0.25f, 0.5f}
                : gui::Color{0.90f, 0.25f, 0.35f, 0.5f});

            renderer.beginFrame(viewport);
            renderer.clear({0.08f, 0.08f, 0.08f, 1.0f});

            frame.draw(renderer);
            sidebar.draw(renderer);
            content.draw(renderer);
            overlay.draw(renderer);

            renderer.endFrame();

            window.swapBuffers();
        }

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
