#include "gui/Event.hpp"

namespace gui {

namespace {

EventType keyEventType(KeyAction action) {
    switch (action) {
        case KeyAction::Press: return EventType::KeyPressed;
        case KeyAction::Release: return EventType::KeyReleased;
        case KeyAction::Repeat: return EventType::KeyRepeated;
    }

    return EventType::KeyPressed;
}

EventType mouseButtonEventType(ButtonAction action) {
    return action == ButtonAction::Press
        ? EventType::MouseButtonPressed
        : EventType::MouseButtonReleased;
}

} // namespace

Event::Event(const KeyEvent& event)
    : type_{keyEventType(event.action)}, data_{event} {}

Event::Event(const MouseMoveEvent& event)
    : type_{EventType::MouseMoved}, data_{event} {}

Event::Event(const MouseButtonEvent& event)
    : type_{mouseButtonEventType(event.action)}, data_{event} {}

Event::Event(const MouseScrollEvent& event)
    : type_{EventType::MouseScrolled}, data_{event} {}

Event::Event(const WindowResizeEvent& event)
    : type_{EventType::WindowResized}, data_{event} {}

Event::Event(const WindowCloseEvent& event)
    : type_{EventType::WindowClosed}, data_{event} {}

Event::Event(const TextInputEvent& event)
    : type_{EventType::TextInput}, data_{event} {}

Event::Event(const WindowFocusEvent& event)
    : type_{EventType::WindowFocusChanged}, data_{event} {}

Event::Event(const CursorEnterEvent& event)
    : type_{EventType::CursorEntered}, data_{event} {}

EventType Event::type() const {
    return type_;
}

} // namespace gui
