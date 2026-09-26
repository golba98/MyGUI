#pragma once

#include "gui/Input.hpp"

#include <variant>

namespace gui {

enum class EventType {
    KeyPressed,
    KeyReleased,
    KeyRepeated,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseScrolled,
    WindowResized,
    WindowClosed
};

struct KeyEvent {
    Key key{Key::Unknown};
    int scancode{0};
    KeyAction action{KeyAction::Press};
    Modifiers mods{};
};

// Cursor position in window content-area coordinates (not framebuffer pixels).
struct MouseMoveEvent {
    double x{0.0};
    double y{0.0};
};

struct MouseButtonEvent {
    MouseButton button{MouseButton::Left};
    ButtonAction action{ButtonAction::Press};
    double x{0.0};
    double y{0.0};
    Modifiers mods{};
};

struct MouseScrollEvent {
    double xOffset{0.0};
    double yOffset{0.0};
};

// New framebuffer size in pixels, matching Window::getWidth/getHeight.
struct WindowResizeEvent {
    int width{0};
    int height{0};
};

struct WindowCloseEvent {};

// A single input or window event. The type is derived from the payload, so
// the two can never disagree. Use type() to switch and getIf<T>() to read.
class Event {
public:
    explicit Event(const KeyEvent& event);
    explicit Event(const MouseMoveEvent& event);
    explicit Event(const MouseButtonEvent& event);
    explicit Event(const MouseScrollEvent& event);
    explicit Event(const WindowResizeEvent& event);
    explicit Event(const WindowCloseEvent& event);

    EventType type() const;

    // Returns the payload if it is of type T, otherwise nullptr.
    template <typename T>
    const T* getIf() const {
        return std::get_if<T>(&data_);
    }

private:
    EventType type_;
    std::variant<
        KeyEvent,
        MouseMoveEvent,
        MouseButtonEvent,
        MouseScrollEvent,
        WindowResizeEvent,
        WindowCloseEvent
    > data_;
};

} // namespace gui
