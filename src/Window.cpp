#include "gui/Window.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <utility>

namespace gui {

Window::Window(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    window_ = glfwCreateWindow(
        width,
        height,
        title.c_str(),
        nullptr,
        nullptr
    );

    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }

    glfwTerminate();
}

Window::Window(Window&& other) noexcept
    : window_{std::exchange(other.window_, nullptr)} {
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (window_) {
            glfwDestroyWindow(window_);
        }

        window_ = std::exchange(other.window_, nullptr);
    }

    return *this;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    glfwSwapBuffers(window_);
}

GLFWwindow* Window::nativeHandle() const {
    return window_;
}

} // namespace gui
