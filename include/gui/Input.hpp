#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace gui {

// Keyboard keys. The numeric values deliberately mirror GLFW's key codes so
// the platform layer can convert with a cast, but nothing here depends on GLFW.
enum class Key : int {
    Unknown = -1,

    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,

    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Semicolon = 59,
    Equal = 61,

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,
    World1 = 161,
    World2 = 162,

    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13,
    F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,

    Keypad0 = 320, Keypad1, Keypad2, Keypad3, Keypad4,
    Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
    KeypadDecimal = 330,
    KeypadDivide = 331,
    KeypadMultiply = 332,
    KeypadSubtract = 333,
    KeypadAdd = 334,
    KeypadEnter = 335,
    KeypadEqual = 336,

    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348
};

enum class KeyAction {
    Press,
    Release,
    Repeat
};

// Mouse buttons. Values mirror GLFW's button indices.
enum class MouseButton : int {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7
};

enum class ButtonAction {
    Press,
    Release
};

// Modifier keys held down when a key or mouse button event occurred.
struct Modifiers {
    bool shift{false};
    bool control{false};
    bool alt{false};
    bool super{false};
};

// Human-readable names, mainly for logging and debugging.
std::string_view toString(Key key);
std::string_view toString(MouseButton button);

class Event;

struct MousePosition {
    double x{0.0};
    double y{0.0};
};

// Persistent state fed by framework events. Pressed/released edges and scroll
// offsets reset at beginFrame(), called by Window::pollEvents(). Both edges can
// be true when a button changes state multiple times in one frame; down always
// reflects its latest state.
class Input {
public:
    // Platform-independent feeding API. Window exposes its Input as const.
    // Start a frame before feeding its events; held state and position persist.
    void beginFrame();
    void processEvent(const Event& event);

    // Cursor position in window content-area coordinates (not framebuffer pixels).
    MousePosition mousePosition() const;
    double mouseX() const;
    double mouseY() const;

    // Scroll offset accumulated during the current frame.
    double scrollX() const;
    double scrollY() const;

    bool isKeyDown(Key key) const;
    bool isKeyPressed(Key key) const;
    bool isKeyReleased(Key key) const;

    bool isMouseDown(MouseButton button) const;
    bool isMouseButtonDown(MouseButton button) const;
    bool isMousePressed(MouseButton button) const;
    bool isMouseReleased(MouseButton button) const;

private:
    struct ButtonState {
        bool down{false};
        bool pressed{false};
        bool released{false};
    };

    static constexpr std::size_t keyCount = static_cast<std::size_t>(Key::Menu) + 1;
    static constexpr std::size_t mouseButtonCount = static_cast<std::size_t>(MouseButton::Button8) + 1;

    void onKey(Key key, KeyAction action);
    void onMouseButton(MouseButton button, ButtonAction action);
    void onMouseMove(double x, double y);
    void onScroll(double xOffset, double yOffset);

    std::array<ButtonState, keyCount> keys_{};
    std::array<ButtonState, mouseButtonCount> mouseButtons_{};
    double mouseX_{0.0};
    double mouseY_{0.0};
    double scrollX_{0.0};
    double scrollY_{0.0};
};

} // namespace gui
