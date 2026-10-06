#include "gui/Window.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <utility>

namespace gui {

namespace {

// gui::Key and gui::MouseButton mirror GLFW's codes; keep them in sync.
static_assert(static_cast<int>(Key::Unknown) == GLFW_KEY_UNKNOWN);
static_assert(static_cast<int>(Key::Space) == GLFW_KEY_SPACE);
static_assert(static_cast<int>(Key::A) == GLFW_KEY_A);
static_assert(static_cast<int>(Key::Z) == GLFW_KEY_Z);
static_assert(static_cast<int>(Key::Escape) == GLFW_KEY_ESCAPE);
static_assert(static_cast<int>(Key::F25) == GLFW_KEY_F25);
static_assert(static_cast<int>(Key::Keypad9) == GLFW_KEY_KP_9);
static_assert(static_cast<int>(Key::Menu) == GLFW_KEY_LAST);
static_assert(static_cast<int>(MouseButton::Left) == GLFW_MOUSE_BUTTON_LEFT);
static_assert(static_cast<int>(MouseButton::Right) == GLFW_MOUSE_BUTTON_RIGHT);
static_assert(static_cast<int>(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);
static_assert(static_cast<int>(MouseButton::Button8) == GLFW_MOUSE_BUTTON_LAST);

Window* windowFrom(GLFWwindow* window) {
    return static_cast<Window*>(glfwGetWindowUserPointer(window));
}

std::optional<KeyAction> toKeyAction(int action) {
    switch (action) {
        case GLFW_RELEASE: return KeyAction::Release;
        case GLFW_REPEAT: return KeyAction::Repeat;
        case GLFW_PRESS: return KeyAction::Press;
        default: return std::nullopt;
    }
}

std::optional<ButtonAction> toButtonAction(int action) {
    switch (action) {
        case GLFW_PRESS: return ButtonAction::Press;
        case GLFW_RELEASE: return ButtonAction::Release;
        default: return std::nullopt;
    }
}

Key toKey(int key) {
    const auto translated = static_cast<Key>(key);
    return toString(translated) == "Unknown" ? Key::Unknown : translated;
}

Modifiers toModifiers(int mods) {
    return Modifiers{
        .shift = (mods & GLFW_MOD_SHIFT) != 0,
        .control = (mods & GLFW_MOD_CONTROL) != 0,
        .alt = (mods & GLFW_MOD_ALT) != 0,
        .super = (mods & GLFW_MOD_SUPER) != 0
    };
}

} // namespace

Window::Window(int width, int height, const std::string& title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // The renderer targets the OpenGL 3.3 core profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

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
    glfwSetKeyCallback(window_, &Window::keyCallback);
    glfwSetCharCallback(window_, &Window::charCallback);
    glfwSetWindowSizeCallback(window_, &Window::windowSizeCallback);
    glfwSetCursorPosCallback(window_, &Window::cursorPosCallback);
    glfwSetMouseButtonCallback(window_, &Window::mouseButtonCallback);
    glfwSetScrollCallback(window_, &Window::scrollCallback);
    glfwSetWindowCloseCallback(window_, &Window::windowCloseCallback);

    // Seed the cursor position; otherwise it reads 0,0 until the mouse moves.
    double cursorX{};
    double cursorY{};
    glfwGetCursorPos(window_, &cursorX, &cursorY);
    input_.processEvent(Event{MouseMoveEvent{.x = cursorX, .y = cursorY}});

    // Query the real framebuffer size; it may not match width/height on HiDPI.
    int framebufferWidth{};
    int framebufferHeight{};

    glfwGetFramebufferSize(
        window_,
        &framebufferWidth,
        &framebufferHeight
    );

    width_ = framebufferWidth;
    height_ = framebufferHeight;
    glViewport(0, 0, width_, height_);
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
      height_{other.height_},
      eventCallback_{std::move(other.eventCallback_)},
      input_{other.input_},
      events_{std::move(other.events_)},
      pendingException_{std::move(other.pendingException_)} {
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
        eventCallback_ = std::move(other.eventCallback_);
        input_ = other.input_;
        events_ = std::move(other.events_);
        pendingException_ = std::move(other.pendingException_);

        if (window_) {
            glfwSetWindowUserPointer(window_, this);
        }
    }

    return *this;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::pollEvents() {
    // Clear last frame's pressed/released edges before new events arrive.
    input_.beginFrame();
    glfwPollEvents();
    if (pendingException_) {
        std::rethrow_exception(std::exchange(pendingException_, nullptr));
    }
}

void Window::swapBuffers() const {
    glfwSwapBuffers(window_);
}

GLProcLoader Window::glProcLoader() const {
    return glfwGetProcAddress;
}

std::optional<Event> Window::nextEvent() {
    if (events_.empty()) {
        return std::nullopt;
    }
    Event event = std::move(events_.front());
    events_.pop_front();
    return event;
}

int Window::getWidth() const {
    return width_;
}

int Window::getHeight() const {
    return height_;
}

Viewport Window::viewport() const {
    int logicalWidth{};
    int logicalHeight{};
    glfwGetWindowSize(window_, &logicalWidth, &logicalHeight);

    return Viewport{
        .logicalWidth = logicalWidth,
        .logicalHeight = logicalHeight,
        .framebufferWidth = width_,
        .framebufferHeight = height_
    };
}

void Window::setEventCallback(EventCallback callback) {
    eventCallback_ = std::move(callback);
}

const Input& Window::input() const {
    return input_;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = windowFrom(window);

    if (self) {
        self->onFramebufferResize(width, height);
    }
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = windowFrom(window);
    const auto translatedAction = toKeyAction(action);

    if (self && translatedAction) {
        const KeyEvent event{
            .key = toKey(key),
            .scancode = scancode,
            .action = *translatedAction,
            .mods = toModifiers(mods)
        };

        self->emit(Event{event});
    }
}

void Window::cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* self = windowFrom(window);

    if (self) {
        self->emit(Event{MouseMoveEvent{.x = x, .y = y}});
    }
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = windowFrom(window);
    const auto translatedAction = toButtonAction(action);

    if (self && translatedAction && button >= GLFW_MOUSE_BUTTON_1
        && button <= GLFW_MOUSE_BUTTON_LAST) {
        double x{};
        double y{};
        glfwGetCursorPos(window, &x, &y);

        const MouseButtonEvent event{
            .button = static_cast<MouseButton>(button),
            .action = *translatedAction,
            .x = x,
            .y = y,
            .mods = toModifiers(mods)
        };

        self->emit(Event{event});
    }
}

void Window::scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    auto* self = windowFrom(window);

    if (self) {
        double x{};
        double y{};
        glfwGetCursorPos(window, &x, &y);
        self->emit(Event{MouseScrollEvent{
            .xOffset = xOffset, .yOffset = yOffset, .x = x, .y = y
        }});
    }
}

void Window::windowCloseCallback(GLFWwindow* window) {
    auto* self = windowFrom(window);

    // Only notifies; GLFW has already set the should-close flag.
    if (self) {
        self->emit(Event{WindowCloseEvent{}});
    }
}

void Window::onFramebufferResize(int width, int height) {
    width_ = width;
    height_ = height;

    glViewport(0, 0, width, height);

    const auto size = viewport();
    emitResize(size.logicalWidth, size.logicalHeight);
}

void Window::windowSizeCallback(GLFWwindow* window, int width, int height) {
    if (auto* self = windowFrom(window)) {
        self->emitResize(width, height);
    }
}

void Window::charCallback(GLFWwindow* window, unsigned int codepoint) {
    // GLFW supplies Unicode scalar values; reject invalid synthetic input too.
    if (auto* self = windowFrom(window); self && codepoint <= 0x10FFFF
        && !(codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        self->emit(Event{TextInputEvent{.codepoint = static_cast<char32_t>(codepoint)}});
    }
}

void Window::emitResize(int logicalWidth, int logicalHeight) {
    emit(Event{WindowResizeEvent{
        .width = width_, .height = height_,
        .logicalWidth = logicalWidth, .logicalHeight = logicalHeight
    }});
}

void Window::emit(const Event& event) noexcept {
    try {
        input_.processEvent(event);
        events_.push_back(event);
        if (eventCallback_) {
            eventCallback_(event);
        }
    }
    catch (...) {
        if (!pendingException_) {
            pendingException_ = std::current_exception();
        }
    }
}

} // namespace gui
