#include "gui/Renderer.hpp"
#include "gui/Window.hpp"

#include <GLFW/glfw3.h>

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
            std::cout << "Window resized: " << resize->width << ", " << resize->height << '\n';
            break;
        }
        case gui::EventType::WindowClosed:
            std::cout << "Window closed\n";
            break;
    }
}

} // namespace

int main() {
    try {
        gui::Window window{1280, 720, "MyGUI"};

        window.setEventCallback(printEvent);

        // Declared after window so it is destroyed while the context is current.
        gui::Renderer renderer{glfwGetProcAddress};

        while (!window.shouldClose()) {
            const float width = static_cast<float>(window.getWidth());
            const float height = static_cast<float>(window.getHeight());

            renderer.beginFrame(window.getWidth(), window.getHeight());

            // Background covering the whole framebuffer.
            renderer.drawRect({0.0f, 0.0f, width, height}, {0.08f, 0.08f, 0.08f, 1.0f});

            // Panel inset from every edge; it tracks the window size.
            renderer.drawRect({40.0f, 40.0f, width - 80.0f, height - 80.0f}, {0.18f, 0.19f, 0.22f, 1.0f});

            // Accent anchored to the top-left corner.
            renderer.drawRect({80.0f, 80.0f, 240.0f, 120.0f}, {0.95f, 0.65f, 0.15f, 1.0f});

            // Opaque rectangle anchored to the bottom-right corner.
            renderer.drawRect({width - 380.0f, height - 280.0f, 280.0f, 180.0f}, {0.20f, 0.45f, 0.90f, 1.0f});

            // Semi-transparent rectangle overlapping the one above and the panel.
            renderer.drawRect({width - 480.0f, height - 360.0f, 280.0f, 180.0f}, {0.90f, 0.25f, 0.35f, 0.5f});

            renderer.endFrame();

            window.swapBuffers();
            window.pollEvents();
        }

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
