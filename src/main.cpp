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

        while (!window.shouldClose()) {
            glClearColor(
                0.08f,
                0.08f,
                0.08f,
                1.0f
            );

            glClear(GL_COLOR_BUFFER_BIT);

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
