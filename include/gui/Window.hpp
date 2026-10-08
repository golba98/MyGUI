#pragma once

#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Viewport.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

struct GLFWwindow;

namespace gui {

enum class EventDelivery { Queue, Callback, Both };

// Window operations and event consumption belong on the main thread.
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
    // Global poll: resets input edges for all windows. Call once per app frame.
    void pollEvents();
    void makeContextCurrent() const;
    void swapBuffers() const;
    // Applies to this window; also makes its context current.
    void setSwapInterval(int interval) const;

    // Requires this window's OpenGL context to be current when used.
    GLProcLoader glProcLoader() const;

    // FIFO delivery when queueing is enabled. Unread events survive polling.
    // State reflects all received events, not the event being removed here.
    std::optional<Event> nextEvent();

    // Framebuffer size in pixels. On HiDPI displays this can differ
    // from the logical window size requested in the constructor.
    int getWidth() const;
    int getHeight() const;

    // Logical (window) size together with framebuffer size. Widgets and
    // mouse coordinates use the logical space.
    Viewport viewport() const;

    // When callback delivery is enabled, receives events after state update on the main
    // thread. GLFW may deliver callbacks outside pollEvents(). Using both
    // delivery APIs receives each event twice. Do not poll recursively.
    // Handler exceptions are deferred until pollEvents(), never through GLFW.
    void setEventCallback(EventCallback callback);
    // Both is the default. Switching to Callback discards unread queued events.
    void setEventDelivery(EventDelivery delivery);
    EventDelivery eventDelivery() const;

    // Polled keyboard and mouse state. Refreshed by each pollEvents() call.
    const Input& input() const;

private:
    // GLFW is a C library and cannot call member functions directly, so these
    // static functions receive the events and forward them to the owning Window
    // found through the GLFW window user pointer.
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void windowSizeCallback(GLFWwindow* window, int width, int height);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void windowCloseCallback(GLFWwindow* window);

    static void focusCallback(GLFWwindow* window, int focused);
    static void cursorEnterCallback(GLFWwindow* window, int entered);

    void onFramebufferResize(int width, int height);
    void emitResize(int logicalWidth, int logicalHeight);
    void emit(const Event& event) noexcept;

    struct State;
    std::unique_ptr<State> state_;
};

} // namespace gui
