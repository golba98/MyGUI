// Pure framework tests: no GLFW, OpenGL, or window creation.
#include "gui/Event.hpp"
#include "gui/Geometry.hpp"
#include "gui/Viewport.hpp"

#include <cstdio>
#include <exception>
#include <initializer_list>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

template <typename T>
const T& payload(const gui::Event& event) {
    const auto* value = event.getIf<T>();
    if (!value) throw std::runtime_error("Unexpected event payload");
    return *value;
}

void key(gui::Input& input, gui::Key value, gui::KeyAction action) {
    input.processEvent(gui::Event{gui::KeyEvent{.key = value, .action = action}});
}

void button(gui::Input& input, gui::MouseButton value, gui::ButtonAction action) {
    input.processEvent(gui::Event{gui::MouseButtonEvent{.button = value, .action = action}});
}

void testEvents() {
    const gui::Modifiers mods{true, true, true, true};
    for (const auto action : {gui::KeyAction::Press, gui::KeyAction::Repeat, gui::KeyAction::Release}) {
        const gui::Event event{gui::KeyEvent{gui::Key::A, 42, action, mods}};
        const auto expected = action == gui::KeyAction::Press ? gui::EventType::KeyPressed
            : action == gui::KeyAction::Repeat ? gui::EventType::KeyRepeated : gui::EventType::KeyReleased;
        const auto& data = payload<gui::KeyEvent>(event);
        check(event.type() == expected && data.key == gui::Key::A && data.scancode == 42
            && data.action == action && data.mods.shift && data.mods.control
            && data.mods.alt && data.mods.super, "key event preserves action, key, scancode, modifiers");
        check(event.getIf<gui::MouseMoveEvent>() == nullptr, "mismatched payload returns nullptr");
    }
    const gui::Event move{gui::MouseMoveEvent{12.25, -3.5}};
    check(move.type() == gui::EventType::MouseMoved && payload<gui::MouseMoveEvent>(move).x == 12.25
        && payload<gui::MouseMoveEvent>(move).y == -3.5, "movement preserves fractional coordinates");
    for (const auto action : {gui::ButtonAction::Press, gui::ButtonAction::Release}) {
        const gui::Event event{gui::MouseButtonEvent{gui::MouseButton::Right, action, 15.5, 25.25, mods}};
        const auto& data = payload<gui::MouseButtonEvent>(event);
        check(event.type() == (action == gui::ButtonAction::Press
            ? gui::EventType::MouseButtonPressed : gui::EventType::MouseButtonReleased)
            && data.button == gui::MouseButton::Right && data.action == action
            && data.x == 15.5 && data.y == 25.25 && data.mods.shift && data.mods.control
            && data.mods.alt && data.mods.super, "button event preserves coordinates and modifiers");
    }
    const gui::Event scroll{gui::MouseScrollEvent{-0.5, 1.25, 20.5, 30.25}};
    const auto& offsets = payload<gui::MouseScrollEvent>(scroll);
    check(scroll.type() == gui::EventType::MouseScrolled && offsets.xOffset == -0.5
        && offsets.yOffset == 1.25 && offsets.x == 20.5 && offsets.y == 30.25,
        "scroll event preserves offsets and cursor position");
    const gui::Event resize{gui::WindowResizeEvent{1200, 900, 800, 600}};
    const auto& size = payload<gui::WindowResizeEvent>(resize);
    check(resize.type() == gui::EventType::WindowResized && size.width == 1200 && size.height == 900
        && size.logicalWidth == 800 && size.logicalHeight == 600, "resize preserves both coordinate spaces");
    const gui::Event close{gui::WindowCloseEvent{}};
    check(close.type() == gui::EventType::WindowClosed && close.getIf<gui::WindowCloseEvent>(),
        "close event construction");
    for (const char32_t codepoint : {U'A', U'\u00E9', U'\U0001F642'}) {
        const gui::Event text{gui::TextInputEvent{codepoint}};
        const gui::Event copy = text;
        check(copy.type() == gui::EventType::TextInput
            && payload<gui::TextInputEvent>(copy).codepoint == codepoint, "text preserves full Unicode code point");
    }
}

void testKeys() {
    gui::Input input;
    check(!input.isKeyDown(gui::Key::A) && !input.isKeyPressed(gui::Key::A)
        && !input.isKeyReleased(gui::Key::A), "keys initially up without edges");
    key(input, gui::Key::A, gui::KeyAction::Press);
    key(input, gui::Key::LeftShift, gui::KeyAction::Press);
    check(input.isKeyDown(gui::Key::A) && input.isKeyPressed(gui::Key::A)
        && !input.isKeyReleased(gui::Key::A) && input.isKeyDown(gui::Key::LeftShift), "press tracks simultaneous keys");
    input.beginFrame();
    check(input.isKeyDown(gui::Key::A) && !input.isKeyPressed(gui::Key::A), "held key survives frame boundary");
    key(input, gui::Key::A, gui::KeyAction::Repeat);
    key(input, gui::Key::A, gui::KeyAction::Press);
    check(input.isKeyDown(gui::Key::A) && !input.isKeyPressed(gui::Key::A)
        && !input.isKeyReleased(gui::Key::A), "repeat and duplicate press do not generate edges");
    key(input, gui::Key::A, gui::KeyAction::Release);
    check(!input.isKeyDown(gui::Key::A) && input.isKeyReleased(gui::Key::A)
        && input.isKeyDown(gui::Key::LeftShift), "release clears only the released key");
    input.beginFrame();
    key(input, gui::Key::A, gui::KeyAction::Release);
    check(!input.isKeyReleased(gui::Key::A), "release edge resets and duplicate release is ignored");
    key(input, gui::Key::A, gui::KeyAction::Press);
    key(input, gui::Key::A, gui::KeyAction::Release);
    check(input.isKeyPressed(gui::Key::A) && input.isKeyReleased(gui::Key::A)
        && !input.isKeyDown(gui::Key::A), "same-frame key press and release preserve both edges");
    key(input, gui::Key::A, gui::KeyAction::Press);
    check(input.isKeyDown(gui::Key::A) && input.isKeyReleased(gui::Key::A), "same-frame re-press preserves release edge");
    for (const auto value : {gui::Key::Unknown, static_cast<gui::Key>(0), static_cast<gui::Key>(64),
                            static_cast<gui::Key>(349), static_cast<gui::Key>(-100)}) {
        key(input, value, gui::KeyAction::Press);
        check(!input.isKeyDown(value) && !input.isKeyPressed(value) && !input.isKeyReleased(value),
            "unknown, gaps and out-of-range keys are safely untracked");
    }
    input.beginFrame();
    key(input, gui::Key::A, static_cast<gui::KeyAction>(99));
    check(input.isKeyDown(gui::Key::A) && !input.isKeyReleased(gui::Key::A), "invalid key action does not change state");
}

void testButtons() {
    gui::Input input;
    check(!input.isMouseButtonDown(gui::MouseButton::Left) && !input.isMousePressed(gui::MouseButton::Left)
        && !input.isMouseReleased(gui::MouseButton::Left), "mouse buttons initially up without edges");
    button(input, gui::MouseButton::Left, gui::ButtonAction::Press);
    button(input, gui::MouseButton::Button8, gui::ButtonAction::Press);
    check(input.isMouseButtonDown(gui::MouseButton::Left) && input.isMouseDown(gui::MouseButton::Left)
        && input.isMousePressed(gui::MouseButton::Left) && input.isMouseDown(gui::MouseButton::Button8),
        "mouse press tracks simultaneous buttons and query alias");
    input.beginFrame();
    button(input, gui::MouseButton::Left, gui::ButtonAction::Press);
    check(input.isMouseDown(gui::MouseButton::Left) && !input.isMousePressed(gui::MouseButton::Left),
        "held mouse survives frame boundary and duplicate press");
    button(input, gui::MouseButton::Left, gui::ButtonAction::Release);
    check(!input.isMouseDown(gui::MouseButton::Left) && input.isMouseReleased(gui::MouseButton::Left)
        && input.isMouseDown(gui::MouseButton::Button8), "mouse release clears only released button");
    input.beginFrame();
    button(input, gui::MouseButton::Left, gui::ButtonAction::Release);
    check(!input.isMouseReleased(gui::MouseButton::Left), "mouse release edge resets and duplicate is ignored");
    button(input, gui::MouseButton::Left, gui::ButtonAction::Press);
    button(input, gui::MouseButton::Left, gui::ButtonAction::Release);
    check(input.isMousePressed(gui::MouseButton::Left) && input.isMouseReleased(gui::MouseButton::Left)
        && !input.isMouseDown(gui::MouseButton::Left), "same-frame mouse press and release preserve both edges");
    for (const auto value : {static_cast<gui::MouseButton>(-1), static_cast<gui::MouseButton>(8)}) {
        button(input, value, gui::ButtonAction::Press);
        check(!input.isMouseDown(value) && !input.isMousePressed(value) && !input.isMouseReleased(value),
            "invalid mouse buttons are safely untracked");
    }
    button(input, gui::MouseButton::Button8, static_cast<gui::ButtonAction>(99));
    check(input.isMouseDown(gui::MouseButton::Button8), "invalid mouse action preserves held state");
}

void testPositionAndScroll() {
    gui::Input input;
    check(input.mouseX() == 0.0 && input.mouseY() == 0.0 && input.scrollX() == 0.0
        && input.scrollY() == 0.0, "initial position and scroll are zero");
    input.processEvent(gui::Event{gui::MouseMoveEvent{200.25, 120.5}});
    input.beginFrame();
    check(input.mousePosition().x == 200.25 && input.mousePosition().y == 120.5
        && input.mouseX() == 200.25 && input.mouseY() == 120.5, "logical fractional mouse position survives frames");
    const gui::Rect rect{200.0f, 120.0f, 320.0f, 180.0f};
    for (const gui::Viewport viewport : {gui::Viewport{800, 600, 800, 600}, gui::Viewport{800, 600, 1200, 900}}) {
        input.processEvent(gui::Event{gui::WindowResizeEvent{
            viewport.framebufferWidth, viewport.framebufferHeight, viewport.logicalWidth, viewport.logicalHeight}});
        const auto position = input.mousePosition();
        check(position.x == 200.25 && position.y == 120.5
            && position.x >= rect.x && position.x < rect.x + rect.width
            && position.y >= rect.y && position.y < rect.y + rect.height,
            "normal and HiDPI resize preserve logical hit-test coordinates");
    }
    input.processEvent(gui::Event{gui::MouseMoveEvent{-1.5, -2.25}});
    check(input.mouseX() == -1.5 && input.mouseY() == -2.25, "outside-window coordinates are not clamped");
    input.processEvent(gui::Event{gui::MouseButtonEvent{
        gui::MouseButton::Left, gui::ButtonAction::Press, 40.5, 50.25}});
    check(input.mouseX() == 40.5 && input.mouseY() == 50.25, "button event synchronizes cursor position");
    input.processEvent(gui::Event{gui::MouseScrollEvent{0.5, 1.25, 70.5, 80.25}});
    input.processEvent(gui::Event{gui::MouseScrollEvent{-0.25, -0.5, 71.5, 81.25}});
    check(input.scrollX() == 0.25 && input.scrollY() == 0.75 && input.mouseX() == 71.5
        && input.mouseY() == 81.25, "multiple scroll events accumulate and update cursor position");
    input.processEvent(gui::Event{gui::TextInputEvent{U'\U0001F642'}});
    input.processEvent(gui::Event{gui::WindowCloseEvent{}});
    check(input.scrollY() == 0.75 && input.isMouseDown(gui::MouseButton::Left)
        && input.mouseX() == 71.5, "text and close events do not alter input state");
    input.beginFrame();
    check(input.scrollX() == 0.0 && input.scrollY() == 0.0 && input.mouseX() == 71.5
        && input.isMouseDown(gui::MouseButton::Left), "scroll and edges reset without resetting held state or position");
}

} // namespace

int main() {
    try {
        testEvents();
        testKeys();
        testButtons();
        testPositionAndScroll();
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
    std::printf("%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
