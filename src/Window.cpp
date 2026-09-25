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

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(
        window_,
        &Window::framebufferSizeCallback
    );

    // Query the real framebuffer size; it may not match width/height on HiDPI.
    int framebufferWidth{};
    int framebufferHeight{};

    glfwGetFramebufferSize(
        window_,
        &framebufferWidth,
        &framebufferHeight
    );

    onFramebufferResize(framebufferWidth, framebufferHeight);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

Window::Window(Window&& other) noexcept
    : window_{std::exchange(other.window_, nullptr)},
      width_{other.width_},
      height_{other.height_} {
    // GLFW still points at the moved-from object; redirect it to this one.
    if (window_) {
        glfwSetWindowUserPointer(window_, this);
    }
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (window_) {
            glfwDestroyWindow(window_);
        }

        window_ = std::exchange(other.window_, nullptr);
        width_ = other.width_;
        height_ = other.height_;

        if (window_) {
            glfwSetWindowUserPointer(window_, this);
        }
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

int Window::getWidth() const {
    return width_;
}

int Window::getHeight() const {
    return height_;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (self) {
        self->onFramebufferResize(width, height);
    }
}

void Window::onFramebufferResize(int width, int height) {
    width_ = width;
    height_ = height;

    glViewport(0, 0, width, height);
}

} // namespace gui
