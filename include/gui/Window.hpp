#pragma once

#include <string>

struct GLFWwindow;

namespace gui {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;

    // Framebuffer size in pixels. On HiDPI displays this can differ
    // from the logical window size requested in the constructor.
    int getWidth() const;
    int getHeight() const;

private:
    // GLFW is a C library and cannot call member functions directly, so this
    // static function receives the event and forwards it to the owning Window
    // found through the GLFW window user pointer.
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    void onFramebufferResize(int width, int height);

    GLFWwindow* window_{nullptr};
    int width_{0};
    int height_{0};
};

}
