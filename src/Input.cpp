#include "gui/Input.hpp"
#include "gui/Event.hpp"

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

// Looks up the state for a Key or MouseButton, or nullptr if the value is
// outside the tracked range (e.g. Key::Unknown).
template <typename States, typename Id>
auto* stateFor(States& states, Id id) {
    const auto index = static_cast<int>(id);
    const bool valid = index >= 0 && static_cast<std::size_t>(index) < states.size();

    return valid ? &states[static_cast<std::size_t>(index)] : nullptr;
}

// Records a down/up transition, marking the edge only when the state changes.
void setDown(auto& state, bool down) {
    if (down && !state.down) {
        state.pressed = true;
    }
    else if (!down && state.down) {
        state.released = true;
    }

    state.down = down;
}

bool isDown(const auto* state) {
    return state && state->down;
}

bool isPressed(const auto* state) {
    return state && state->pressed;
}

bool isReleased(const auto* state) {
    return state && state->released;
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

MousePosition Input::mousePosition() const {
    return {.x = mouseX_, .y = mouseY_};
}

double Input::mouseX() const {
    return mouseX_;
}

double Input::mouseY() const {
    return mouseY_;
}

double Input::scrollX() const {
    return scrollX_;
}

double Input::scrollY() const {
    return scrollY_;
}

bool Input::isKeyDown(Key key) const {
    return isDown(stateFor(keys_, key));
}

bool Input::isKeyPressed(Key key) const {
    return isPressed(stateFor(keys_, key));
}

bool Input::isKeyReleased(Key key) const {
    return isReleased(stateFor(keys_, key));
}

bool Input::isMouseDown(MouseButton button) const {
    return isDown(stateFor(mouseButtons_, button));
}

bool Input::isMouseButtonDown(MouseButton button) const {
    return isMouseDown(button);
}

bool Input::isMousePressed(MouseButton button) const {
    return isPressed(stateFor(mouseButtons_, button));
}

bool Input::isMouseReleased(MouseButton button) const {
    return isReleased(stateFor(mouseButtons_, button));
}

void Input::beginFrame() {
    for (auto& key : keys_) {
        key.pressed = false;
        key.released = false;
    }

    for (auto& button : mouseButtons_) {
        button.pressed = false;
        button.released = false;
    }

    scrollX_ = 0.0;
    scrollY_ = 0.0;
}

void Input::onKey(Key key, KeyAction action) {
    if (toString(key) == "Unknown") {
        return;
    }
    auto* state = stateFor(keys_, key);

    // Repeat is not a new press, and the key is already down.
    if (state && (action == KeyAction::Press || action == KeyAction::Release)) {
        setDown(*state, action == KeyAction::Press);
    }
}

void Input::onMouseButton(MouseButton button, ButtonAction action) {
    auto* state = stateFor(mouseButtons_, button);

    if (state && (action == ButtonAction::Press || action == ButtonAction::Release)) {
        setDown(*state, action == ButtonAction::Press);
    }
}

void Input::onMouseMove(double x, double y) {
    mouseX_ = x;
    mouseY_ = y;
}

void Input::onScroll(double xOffset, double yOffset) {
    // Several scroll events can arrive in one poll, so accumulate them.
    scrollX_ += xOffset;
    scrollY_ += yOffset;
}

void Input::processEvent(const Event& event) {
    if (const auto* key = event.getIf<KeyEvent>()) {
        onKey(key->key, key->action);
    }
    else if (const auto* move = event.getIf<MouseMoveEvent>()) {
        onMouseMove(move->x, move->y);
    }
    else if (const auto* button = event.getIf<MouseButtonEvent>()) {
        onMouseMove(button->x, button->y);
        onMouseButton(button->button, button->action);
    }
    else if (const auto* scroll = event.getIf<MouseScrollEvent>()) {
        onMouseMove(scroll->x, scroll->y);
        onScroll(scroll->xOffset, scroll->yOffset);
    }
}

} // namespace gui
