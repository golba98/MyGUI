#include "gui/Input.hpp"

#include <array>

namespace gui {

namespace {

bool inRange(Key key, Key first, Key last) {
    return key >= first && key <= last;
}

// Offset of key from first, for indexing the name tables below.
int offset(Key key, Key first) {
    return static_cast<int>(key) - static_cast<int>(first);
}

} // namespace

std::string_view toString(Key key) {
    static constexpr std::string_view letters{"ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
    static constexpr std::string_view digits{"0123456789"};
    static constexpr std::array<std::string_view, 25> functionKeys{
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
        "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20",
        "F21", "F22", "F23", "F24", "F25"
    };
    static constexpr std::array<std::string_view, 10> keypadDigits{
        "Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4",
        "Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9"
    };

    if (inRange(key, Key::A, Key::Z)) {
        return letters.substr(offset(key, Key::A), 1);
    }

    if (inRange(key, Key::Num0, Key::Num9)) {
        return digits.substr(offset(key, Key::Num0), 1);
    }

    if (inRange(key, Key::F1, Key::F25)) {
        return functionKeys[offset(key, Key::F1)];
    }

    if (inRange(key, Key::Keypad0, Key::Keypad9)) {
        return keypadDigits[offset(key, Key::Keypad0)];
    }

    switch (key) {
        case Key::Space: return "Space";
        case Key::Apostrophe: return "'";
        case Key::Comma: return ",";
        case Key::Minus: return "-";
        case Key::Period: return ".";
        case Key::Slash: return "/";
        case Key::Semicolon: return ";";
        case Key::Equal: return "=";
        case Key::LeftBracket: return "[";
        case Key::Backslash: return "\\";
        case Key::RightBracket: return "]";
        case Key::GraveAccent: return "`";
        case Key::World1: return "World 1";
        case Key::World2: return "World 2";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";
        case Key::PageUp: return "Page Up";
        case Key::PageDown: return "Page Down";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::CapsLock: return "Caps Lock";
        case Key::ScrollLock: return "Scroll Lock";
        case Key::NumLock: return "Num Lock";
        case Key::PrintScreen: return "Print Screen";
        case Key::Pause: return "Pause";
        case Key::KeypadDecimal: return "Keypad .";
        case Key::KeypadDivide: return "Keypad /";
        case Key::KeypadMultiply: return "Keypad *";
        case Key::KeypadSubtract: return "Keypad -";
        case Key::KeypadAdd: return "Keypad +";
        case Key::KeypadEnter: return "Keypad Enter";
        case Key::KeypadEqual: return "Keypad =";
        case Key::LeftShift: return "Left Shift";
        case Key::LeftControl: return "Left Control";
        case Key::LeftAlt: return "Left Alt";
        case Key::LeftSuper: return "Left Super";
        case Key::RightShift: return "Right Shift";
        case Key::RightControl: return "Right Control";
        case Key::RightAlt: return "Right Alt";
        case Key::RightSuper: return "Right Super";
        case Key::Menu: return "Menu";
        default: return "Unknown";
    }
}

std::string_view toString(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Right: return "Right";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Button4: return "Button 4";
        case MouseButton::Button5: return "Button 5";
        case MouseButton::Button6: return "Button 6";
        case MouseButton::Button7: return "Button 7";
        case MouseButton::Button8: return "Button 8";
    }

    return "Unknown";
}

} // namespace gui
