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

    GLFWwindow* nativeHandle() const;

private:
    GLFWwindow* window_{nullptr};
};

}
