#include "gui/Window.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <list>
#include <vector>

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

struct Window::State {
    GLFWwindow* window_{nullptr};
    int width_{0}, height_{0};
    EventCallback eventCallback_;
    Input input_;
    std::list<Event> events_;
    std::exception_ptr pendingException_;
    EventDelivery delivery_{EventDelivery::Both};

    static std::vector<State*>& live() {
        static std::vector<State*> windows;
        return windows;
    }
    State() {
        const bool first = live().empty();
        if (first && !glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
        try { live().push_back(this); }
        catch (...) { if (first) glfwTerminate(); throw; }
    }
    ~State() {
        if (window_) glfwDestroyWindow(window_);
        auto& windows = live();
        windows.erase(std::find(windows.begin(), windows.end(), this));
        if (windows.empty()) glfwTerminate();
    }
};

Window::Window(int width, int height, const std::string& title) : state_{std::make_unique<State>()} {

    if (width <= 0 || height <= 0) throw std::invalid_argument("Window dimensions must be positive");

    // The renderer targets the OpenGL 3.3 core profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    state_->window_ = glfwCreateWindow(
        width,
        height,
        title.c_str(),
        nullptr,
        nullptr
    );

    if (!state_->window_) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(state_->window_);

    glfwSetWindowUserPointer(state_->window_, this);
    glfwSetFramebufferSizeCallback(
        state_->window_,
        &Window::framebufferSizeCallback
    );
    glfwSetKeyCallback(state_->window_, &Window::keyCallback);
    glfwSetCharCallback(state_->window_, &Window::charCallback);
    glfwSetWindowSizeCallback(state_->window_, &Window::windowSizeCallback);
    glfwSetCursorPosCallback(state_->window_, &Window::cursorPosCallback);
    glfwSetMouseButtonCallback(state_->window_, &Window::mouseButtonCallback);
    glfwSetScrollCallback(state_->window_, &Window::scrollCallback);
    glfwSetWindowCloseCallback(state_->window_, &Window::windowCloseCallback);
    glfwSetWindowFocusCallback(state_->window_, &Window::focusCallback);
    glfwSetCursorEnterCallback(state_->window_, &Window::cursorEnterCallback);

    // Seed the cursor position; otherwise it reads 0,0 until the mouse moves.
    double cursorX{};
    double cursorY{};
    glfwGetCursorPos(state_->window_, &cursorX, &cursorY);
    state_->input_.processEvent(Event{MouseMoveEvent{.x = cursorX, .y = cursorY}});

    // Query the real framebuffer size; it may not match width/height on HiDPI.
    int framebufferWidth{};
    int framebufferHeight{};

    glfwGetFramebufferSize(
        state_->window_,
        &framebufferWidth,
        &framebufferHeight
    );

    state_->width_ = framebufferWidth;
    state_->height_ = framebufferHeight;
}

Window::~Window() = default;

Window::Window(Window&& other) noexcept : state_{std::move(other.state_)} {
    if (state_) glfwSetWindowUserPointer(state_->window_, this);
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
        if (state_) glfwSetWindowUserPointer(state_->window_, this);
    }
    return *this;
}

bool Window::shouldClose() const {
    if (!state_) return true;
    return glfwWindowShouldClose(state_->window_);
}

void Window::pollEvents() {
    static bool polling = false;
    if (polling) throw std::logic_error("Recursive event polling is not allowed");
    struct Guard { bool& flag; ~Guard() { flag = false; } } guard{polling};
    polling = true;
    for (auto* state : State::live()) state->input_.beginFrame();
    glfwPollEvents();
    for (auto* state : State::live()) {
        if (state->pendingException_) {
            std::rethrow_exception(std::exchange(state->pendingException_, nullptr));
        }
    }
}

void Window::makeContextCurrent() const {
    glfwMakeContextCurrent(state_->window_);
}

void Window::swapBuffers() const {
    glfwSwapBuffers(state_->window_);
}

void Window::setSwapInterval(int interval) const {
    makeContextCurrent();
    glfwSwapInterval(interval);
}

GLProcLoader Window::glProcLoader() const {
    return glfwGetProcAddress;
}

std::optional<Event> Window::nextEvent() {
    if (state_->events_.empty()) {
        return std::nullopt;
    }
    Event event = std::move(state_->events_.front());
    state_->events_.pop_front();
    return event;
}

int Window::getWidth() const {
    return state_->width_;
}

int Window::getHeight() const {
    return state_->height_;
}

Viewport Window::viewport() const {
    int logicalWidth{};
    int logicalHeight{};
    glfwGetWindowSize(state_->window_, &logicalWidth, &logicalHeight);

    return Viewport{
        .logicalWidth = logicalWidth,
        .logicalHeight = logicalHeight,
        .framebufferWidth = state_->width_,
        .framebufferHeight = state_->height_
    };
}

void Window::setEventCallback(EventCallback callback) {
    state_->eventCallback_ = std::move(callback);
}

const Input& Window::input() const {
    return state_->input_;
}

void Window::setEventDelivery(EventDelivery delivery) {
    state_->delivery_ = delivery;
    if (delivery == EventDelivery::Callback) state_->events_.clear();
}

EventDelivery Window::eventDelivery() const { return state_->delivery_; }

void Window::focusCallback(GLFWwindow* window, int focused) {
    if (auto* self = windowFrom(window)) self->emit(Event{WindowFocusEvent{focused != 0}});
}

void Window::cursorEnterCallback(GLFWwindow* window, int entered) {
    if (auto* self = windowFrom(window)) self->emit(Event{CursorEnterEvent{entered != 0}});
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
    state_->width_ = width;
    state_->height_ = height;


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
        .width = state_->width_, .height = state_->height_,
        .logicalWidth = logicalWidth, .logicalHeight = logicalHeight
    }});
}

void Window::emit(const Event& event) noexcept {
    auto* state = state_.get();
    try {
        state->input_.processEvent(event);
        if (state->delivery_ != EventDelivery::Callback) state->events_.push_back(event);
        if (state->delivery_ != EventDelivery::Queue) {
            const auto callback = state->eventCallback_;
            if (callback) callback(event);
        }
    }
    catch (...) {
        if (!state->pendingException_) {
            state->pendingException_ = std::current_exception();
        }
    }
}

} // namespace gui
