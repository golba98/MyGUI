#pragma once

#include "gui/Event.hpp"
#include "gui/Viewport.hpp"

#include <functional>
#include <string>

struct GLFWwindow;

namespace gui {

class Window {
public:
    using EventCallback = std::function<void(const Event&)>;

    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool shouldClose() const;
    void pollEvents();
    void swapBuffers() const;

    // Synchronizes swapBuffers() with the display refresh. On by default, so
    // the render loop does not spin at full speed. Affects the current context.
    void setVSync(bool enabled);

    // Framebuffer size in pixels. On HiDPI displays this can differ
    // from the logical window size requested in the constructor.
    int getWidth() const;
    int getHeight() const;

    // Logical (window) size together with framebuffer size. Widgets and
    // mouse coordinates use the logical space.
    Viewport viewport() const;

    // Receives every input and window event. Events are delivered from
    // inside pollEvents(), on the calling thread.
    void setEventCallback(EventCallback callback);

    // Polled keyboard and mouse state. Refreshed by each pollEvents() call.
    const Input& input() const;

private:
    // GLFW is a C library and cannot call member functions directly, so these
    // static functions receive the events and forward them to the owning Window
    // found through the GLFW window user pointer.
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void windowCloseCallback(GLFWwindow* window);

    void onFramebufferResize(int width, int height);
    void emit(const Event& event) const;

    GLFWwindow* window_{nullptr};
    int width_{0};
    int height_{0};
    EventCallback eventCallback_;
    Input input_;
};

}
