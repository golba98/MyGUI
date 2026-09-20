#include "gui/Window.hpp"

#include <GLFW/glfw3.h>

#include <exception>
#include <iostream>

int main() {
    try {
        gui::Window window{1280, 720, "MyGUI"};

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
