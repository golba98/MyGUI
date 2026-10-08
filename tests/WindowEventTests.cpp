// Integration tests use a hidden window and invoke the callbacks registered
// by Window. Native handles are confined to this test, never application code.
#include "gui/Window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

template <typename Callback>
Callback registered(GLFWwindow* window, Callback (*setter)(GLFWwindow*, Callback)) {
    const auto callback = setter(window, nullptr);
    setter(window, callback);
    if (!callback) throw std::runtime_error("Required GLFW callback was not registered");
    return callback;
}

struct Callbacks {
    GLFWkeyfun key;
    GLFWcharfun character;
    GLFWcursorposfun move;
    GLFWmousebuttonfun button;
    GLFWscrollfun scroll;
    GLFWwindowsizefun size;
    GLFWframebuffersizefun framebuffer;
    GLFWwindowclosefun close;
    GLFWwindowfocusfun focus;
    GLFWcursorenterfun enter;

    explicit Callbacks(GLFWwindow* window)
        : key{registered(window, glfwSetKeyCallback)},
          character{registered(window, glfwSetCharCallback)},
          move{registered(window, glfwSetCursorPosCallback)},
          button{registered(window, glfwSetMouseButtonCallback)},
          scroll{registered(window, glfwSetScrollCallback)},
          size{registered(window, glfwSetWindowSizeCallback)},
          framebuffer{registered(window, glfwSetFramebufferSizeCallback)},
          close{registered(window, glfwSetWindowCloseCallback)},
          focus{registered(window, glfwSetWindowFocusCallback)},
          enter{registered(window, glfwSetCursorEnterCallback)} {}
};

gui::Event next(gui::Window& window) {
    auto event = window.nextEvent();
    if (!event) throw std::runtime_error("Expected queued event");
    return *event;
}

template <typename T>
const T& payload(const gui::Event& event) {
    const auto* data = event.getIf<T>();
    if (!data) throw std::runtime_error("Unexpected event payload");
    return *data;
}

void drain(gui::Window& window) {
    while (window.nextEvent()) {}
}

void testDelivery(gui::Window& window, GLFWwindow* native, const Callbacks& callbacks) {
    std::vector<gui::EventType> delivered;
    window.setEventCallback([&](const gui::Event& event) {
        delivered.push_back(event.type());
        if (const auto* key = event.getIf<gui::KeyEvent>(); key && key->key == gui::Key::A) {
            check(window.input().isKeyDown(key->key) == (key->action != gui::KeyAction::Release),
                "key state updated before handler");
        }
        if (const auto* move = event.getIf<gui::MouseMoveEvent>()) {
            check(window.input().mouseX() == move->x && window.input().mouseY() == move->y,
                "cursor state updated before handler");
        }
        if (const auto* button = event.getIf<gui::MouseButtonEvent>()) {
            check(window.input().isMouseDown(button->button) == (button->action == gui::ButtonAction::Press)
                && window.input().mouseX() == button->x && window.input().mouseY() == button->y,
                "button and sampled cursor state updated before handler");
        }
    });

    const int mods = GLFW_MOD_SHIFT | GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER;
    double x{};
    double y{};
    glfwGetCursorPos(native, &x, &y);
    callbacks.move(native, 200.25, 120.5);
    callbacks.key(native, GLFW_KEY_A, 42, GLFW_PRESS, mods);
    callbacks.key(native, GLFW_KEY_A, 42, GLFW_REPEAT, mods);
    callbacks.character(native, 0x1F642);
    callbacks.button(native, GLFW_MOUSE_BUTTON_MIDDLE, GLFW_PRESS, mods);
    callbacks.scroll(native, -0.5, 1.25);
    callbacks.button(native, GLFW_MOUSE_BUTTON_MIDDLE, GLFW_RELEASE, mods);
    callbacks.key(native, GLFW_KEY_A, 42, GLFW_RELEASE, mods);
    callbacks.key(native, GLFW_KEY_UNKNOWN, 999, GLFW_PRESS, 0);
    callbacks.close(native);

    const std::vector<gui::EventType> expected{
        gui::EventType::MouseMoved, gui::EventType::KeyPressed, gui::EventType::KeyRepeated,
        gui::EventType::TextInput, gui::EventType::MouseButtonPressed, gui::EventType::MouseScrolled,
        gui::EventType::MouseButtonReleased, gui::EventType::KeyReleased,
        gui::EventType::KeyPressed, gui::EventType::WindowClosed
    };
    check(delivered == expected, "callbacks receive translated events in arrival order");
    std::vector<gui::EventType> queued;
    while (auto event = window.nextEvent()) {
        queued.push_back(event->type());
        if (const auto* key = event->getIf<gui::KeyEvent>(); key && key->key == gui::Key::A) {
            check(key->scancode == 42 && key->mods.shift && key->mods.control
                && key->mods.alt && key->mods.super, "GLFW key and modifiers translated without losing scancode");
        }
        if (const auto* key = event->getIf<gui::KeyEvent>(); key && key->key == gui::Key::Unknown) {
            check(key->scancode == 999 && !window.input().isKeyDown(gui::Key::Unknown),
                "unknown key delivered with scancode but not tracked");
        }
        if (const auto* text = event->getIf<gui::TextInputEvent>()) {
            check(text->codepoint == U'\U0001F642', "character callback preserves supplementary Unicode");
        }
        if (const auto* move = event->getIf<gui::MouseMoveEvent>()) {
            check(move->x == 200.25 && move->y == 120.5, "movement payload preserves logical coordinates");
        }
        if (const auto* button = event->getIf<gui::MouseButtonEvent>()) {
            check(button->button == gui::MouseButton::Middle && button->x == x && button->y == y
                && button->mods.shift && button->mods.control && button->mods.alt && button->mods.super,
                "button translation includes sampled logical cursor and modifiers");
        }
        if (const auto* scroll = event->getIf<gui::MouseScrollEvent>()) {
            check(scroll->xOffset == -0.5 && scroll->yOffset == 1.25 && scroll->x == x && scroll->y == y,
                "scroll translation includes offsets and sampled logical cursor");
        }
    }
    check(queued == expected && queued == delivered, "queue and callbacks deliver identical FIFO sequence");
    check(!window.nextEvent(), "empty queue returns nullopt repeatedly");
    check(window.input().isKeyPressed(gui::Key::A) && window.input().isKeyReleased(gui::Key::A)
        && !window.input().isKeyDown(gui::Key::A) && window.input().isMousePressed(gui::MouseButton::Middle)
        && window.input().isMouseReleased(gui::MouseButton::Middle), "draining queue does not replay state transitions");

    window.setEventCallback({});
    callbacks.key(native, GLFW_KEY_A, 0, 99, 0);
    callbacks.button(native, -1, GLFW_PRESS, 0);
    callbacks.button(native, GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT, 0);
    callbacks.character(native, 0xD800);
    callbacks.character(native, 0x110000);
    check(!window.nextEvent(), "invalid platform actions, buttons, and Unicode are ignored");
    callbacks.key(native, 64, 123, GLFW_PRESS, 0);
    const auto unknown = next(window);
    check(payload<gui::KeyEvent>(unknown).key == gui::Key::Unknown,
        "unmapped platform key translates to framework Unknown");

    callbacks.character(native, 'Z');
    window.pollEvents();
    const auto unread = next(window);
    check(payload<gui::TextInputEvent>(unread).codepoint == U'Z', "unread events survive pollEvents");
    check(!window.input().isKeyPressed(gui::Key::A) && !window.input().isKeyReleased(gui::Key::A)
        && !window.input().isMousePressed(gui::MouseButton::Middle) && window.input().scrollY() == 0.0,
        "pollEvents resets frame edges and scrolling");
    drain(window);

    window.setEventCallback([&](const gui::Event&) {
        const auto alreadyQueued = window.nextEvent();
        check(alreadyQueued && alreadyQueued->type() == gui::EventType::TextInput,
            "event is enqueued before handler invocation");
    });
    callbacks.character(native, 'Q');
    check(!window.nextEvent(), "handler can consume queued event");
    window.setEventCallback({});
}

void testResize(gui::Window& window, GLFWwindow* native, const Callbacks& callbacks) {
    const auto initial = window.viewport();
    callbacks.size(native, 500, 400);
    const auto logicalEvent = next(window);
    const auto& logical = payload<gui::WindowResizeEvent>(logicalEvent);
    check(logical.logicalWidth == 500 && logical.logicalHeight == 400
        && logical.width == initial.framebufferWidth && logical.height == initial.framebufferHeight,
        "logical resize callback reports framebuffer and logical dimensions");

    for (const int scale : {1, 2}) {
        const auto getInteger = reinterpret_cast<PFNGLGETINTEGERVPROC>(window.glProcLoader()("glGetIntegerv"));
        if (!getInteger) throw std::runtime_error("Missing glGetIntegerv");
        GLint before[4]{};
        getInteger(GL_VIEWPORT, before);
        callbacks.framebuffer(native, initial.logicalWidth * scale, initial.logicalHeight * scale);
        const auto resize = next(window);
        const auto& size = payload<gui::WindowResizeEvent>(resize);
        const auto viewport = window.viewport();
        check(size.width == initial.logicalWidth * scale && size.height == initial.logicalHeight * scale
            && size.logicalWidth == initial.logicalWidth && size.logicalHeight == initial.logicalHeight
            && window.getWidth() == size.width && window.getHeight() == size.height
            && viewport.framebufferWidth == size.width, "framebuffer resize updates event, getters, and viewport");
        callbacks.move(native, 200.25, 120.5);
        const auto movement = next(window);
        check(payload<gui::MouseMoveEvent>(movement).x == 200.25 && window.input().mouseX() == 200.25
            && window.input().mouseY() == 120.5, "normal and HiDPI framebuffer sizes never rescale mouse coordinates");
        GLint actual[4]{};
        getInteger(GL_VIEWPORT, actual);
        check(actual[0] == before[0] && actual[1] == before[1] && actual[2] == before[2] && actual[3] == before[3],
            "framebuffer callback leaves the current context viewport untouched");
    }
    callbacks.framebuffer(native, 0, 0);
    const auto minimized = next(window);
    check(payload<gui::WindowResizeEvent>(minimized).width == 0 && window.getHeight() == 0,
        "zero framebuffer size is safe");
    callbacks.framebuffer(native, initial.framebufferWidth, initial.framebufferHeight);
    drain(window);

    glfwSetWindowSize(native, 500, 400);
    bool receivedResize = false;
    // X11 window-manager resize replies are asynchronous. Require the actual
    // notification, allowing a bounded interval for the platform to deliver it.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    do {
        window.pollEvents();
        while (auto event = window.nextEvent()) {
            if (const auto* size = event->getIf<gui::WindowResizeEvent>()) {
                receivedResize = receivedResize || (size->logicalWidth == 500 && size->logicalHeight == 400);
            }
        }
        if (!receivedResize) std::this_thread::sleep_for(std::chrono::milliseconds{5});
    } while (!receivedResize && std::chrono::steady_clock::now() < deadline);
    check(receivedResize && window.viewport().logicalWidth == 500 && window.viewport().logicalHeight == 400,
        "actual window resize produces framework resize events");
}

void testDeliveryModes(gui::Window& window, GLFWwindow* native, const Callbacks& callbacks) {
    drain(window);
    int count = 0;
    window.setEventCallback([&](const gui::Event&) { ++count; });
    window.setEventDelivery(gui::EventDelivery::Queue);
    callbacks.character(native, 'Q');
    check(count == 0 && next(window).type() == gui::EventType::TextInput, "queue-only mode bypasses callback");
    callbacks.character(native, 'X');
    window.setEventDelivery(gui::EventDelivery::Callback);
    check(!window.nextEvent(), "callback-only mode discards previous unread queue");
    for (int i = 0; i < 50000; ++i) callbacks.character(native, 'C');
    check(count == 50000 && !window.nextEvent(), "callback-only delivery never accumulates queued events");
    window.setEventCallback([&](const gui::Event&) { ++count; window.setEventCallback({}); });
    callbacks.character(native, 'R');
    callbacks.character(native, 'S');
    check(count == 50001, "callback can safely clear itself during invocation");
    window.setEventDelivery(gui::EventDelivery::Both);
    callbacks.character(native, 'B');
    check(payload<gui::TextInputEvent>(next(window)).codepoint == U'B' && !window.nextEvent(),
          "re-enabling the queue records only future events");
    callbacks.key(native, GLFW_KEY_A, 1, GLFW_PRESS, 0);
    callbacks.button(native, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    callbacks.focus(native, GLFW_FALSE);
    check(!window.input().isKeyDown(gui::Key::A) && !window.input().isMouseDown(gui::MouseButton::Left),
          "focus loss releases held keys and mouse buttons");
    callbacks.enter(native, GLFW_FALSE);
    callbacks.enter(native, GLFW_TRUE);
    callbacks.focus(native, GLFW_TRUE);
    drain(window);
}

void testWindowLifetime(gui::Window& survivor, GLFWwindow* native, const Callbacks& callbacks) {
    drain(survivor);
    {
        gui::Window temporary{100, 100, "temporary"};
        const auto otherNative = glfwGetCurrentContext();
        const Callbacks otherCallbacks{otherNative};
        otherCallbacks.key(otherNative, GLFW_KEY_C, 1, GLFW_PRESS, 0);
        check(temporary.input().isKeyPressed(gui::Key::C), "second window receives independent state");
        survivor.pollEvents();
        check(!temporary.input().isKeyPressed(gui::Key::C), "one global poll resets every window's frame edges");
        bool failed = false;
        try { gui::Window invalid{0, 100, "invalid"}; } catch (const std::invalid_argument&) { failed = true; }
        check(failed && !temporary.shouldClose(), "failed construction leaves live windows usable");
    }
    survivor.makeContextCurrent();
    callbacks.character(native, 'L');
    bool delivered = false;
    while (auto event = survivor.nextEvent()) {
        if (auto* text = event->getIf<gui::TextInputEvent>()) delivered |= text->codepoint == U'L';
    }
    check(delivered && glfwGetCurrentContext() == native && !survivor.shouldClose(),
          "destroying a second window preserves the survivor and its context");
    {
        gui::Window first{100, 100, "first"};
        gui::Window second{100, 100, "second"};
        const auto retained = glfwGetCurrentContext();
        first = std::move(second);
        first.makeContextCurrent();
        check(glfwGetCurrentContext() == retained && !first.shouldClose(), "move assignment releases only the replaced window");
    }
    {
        gui::Window temporary{100, 100, "move inside callback"};
        const auto movingNative = glfwGetCurrentContext();
        const Callbacks movingCallbacks{movingNative};
        std::optional<gui::Window> moved;
        temporary.setEventCallback([&](const gui::Event&) {
            moved.emplace(std::move(temporary));
            throw std::runtime_error("moved callback failure");
        });
        movingCallbacks.character(movingNative, 'M');
        moved->setEventCallback({});
        check(payload<gui::TextInputEvent>(next(*moved)).codepoint == U'M', "move during callback retains the queued event");
        bool caught = false;
        try { survivor.pollEvents(); } catch (const std::runtime_error& error) { caught = std::string{error.what()} == "moved callback failure"; }
        check(caught, "global polling reports exceptions from a window moved during its callback");
    }
    survivor.makeContextCurrent();
    drain(survivor);
}

void testMoveAndExceptions(gui::Window& original, GLFWwindow* native, const Callbacks& callbacks) {
    int notifications = 0;
    original.setEventCallback([&](const gui::Event&) { ++notifications; });
    callbacks.key(native, GLFW_KEY_B, 2, GLFW_PRESS, 0);
    gui::Window moved{std::move(original)};
    check(moved.input().isKeyDown(gui::Key::B) && next(moved).type() == gui::EventType::KeyPressed,
        "move construction transfers held state and pending queue");
    callbacks.key(native, GLFW_KEY_B, 2, GLFW_RELEASE, 0);
    check(!moved.input().isKeyDown(gui::Key::B) && next(moved).type() == gui::EventType::KeyReleased
        && notifications == 2, "callbacks route to moved Window and retain event handler");

    gui::Window assigned{200, 150, "assignment target"};
    callbacks.character(native, 'M');
    assigned = std::move(moved);
    // Destroying the target's old window detaches its context; use the retained
    // source context for the remaining integration checks.
    assigned.makeContextCurrent();
    check(payload<gui::TextInputEvent>(next(assigned)).codepoint == U'M', "move assignment transfers pending queue");
    callbacks.move(native, 17.5, 21.25);
    check(assigned.input().mouseX() == 17.5 && next(assigned).type() == gui::EventType::MouseMoved
        && notifications == 4, "callbacks route correctly after move assignment");
    assigned.setEventCallback([](const gui::Event&) { throw std::runtime_error("handler failure"); });
    callbacks.character(native, 'E');
    check(payload<gui::TextInputEvent>(next(assigned)).codepoint == U'E',
        "throwing handler cannot unwind through GLFW and event remains queued");
    assigned.setEventCallback({});
    bool caught = false;
    try { assigned.pollEvents(); }
    catch (const std::runtime_error& error) { caught = std::string{error.what()} == "handler failure"; }
    check(caught, "handler exception rethrown at pollEvents boundary");
    assigned.pollEvents();
    check(true, "deferred exception is reported only once");
}

} // namespace

int main() {
    static_assert(std::is_nothrow_move_constructible_v<gui::Window>);
    static_assert(std::is_nothrow_move_assignable_v<gui::Window>);
    try {
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        gui::Window window{400, 300, "MyGUI window tests"};
        GLFWwindow* native = glfwGetCurrentContext();
        if (!native) throw std::runtime_error("Missing current context");
        const Callbacks callbacks{native};
        check(!window.nextEvent(), "construction does not enqueue artificial startup events");
        window.pollEvents();
        drain(window);
        testDelivery(window, native, callbacks);
        testResize(window, native, callbacks);
        testDeliveryModes(window, native, callbacks);
        testWindowLifetime(window, native, callbacks);
        testMoveAndExceptions(window, native, callbacks);
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
    std::printf("%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
