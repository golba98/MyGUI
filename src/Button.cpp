#include "gui/Button.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>

namespace gui {
Button::Button() : Button("") {}
Button::Button(const Button& other) : Label(other), style_{other.style_}, onClick_{other.onClick_} {}
Button& Button::operator=(const Button& other) {
    if (this != &other) {
        auto callback = other.onClick_;
        Label::operator=(other);
        style_ = other.style_; onClick_ = std::move(callback); cancelInteraction();
    }
    return *this;
}
Button::Button(std::string text, std::shared_ptr<Font> font) : Label(std::move(text), std::move(font)) {
    setFocusable(true);
    setMinimumSize({80, 32});
    setAlignment(HorizontalAlign::Center, VerticalAlign::Center);
}
Size Button::preferredSize() const noexcept {
    const auto text = Label::preferredSize();
    return {std::max(minimumSize().width, text.width + 24), std::max(minimumSize().height, text.height + 16)};
}
void Button::cancelInteraction() noexcept { pointerPressed_ = false; keyboardKey_ = Key::Unknown; }
void Button::activate() { const auto callback = onClick_; if (callback) callback(); }
bool Button::onEvent(const Event& event, UIContext& context) {
    if (!enabled()) return false;
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && mouse->button == MouseButton::Left) {
        if (mouse->action == ButtonAction::Press) {
            if (!hitTest(mouse->x, mouse->y)) return false;
            cancelInteraction();
            pointerPressed_ = context.capturePointer(*this);
            return pointerPressed_;
        }
        if (pointerPressed_) {
            const bool click = context.hoveredWidget() == this && hitTest(mouse->x, mouse->y);
            pointerPressed_ = false;
            context.releasePointer(*this);
            if (click) activate();
            return true;
        }
    }
    if (event.getIf<MouseMoveEvent>()) return pointerPressed_;
    if (const auto* key = event.getIf<KeyEvent>(); key && focused() && (key->key == Key::Enter || key->key == Key::Space)) {
        if (key->action == KeyAction::Press && keyboardKey_ == Key::Unknown) {
            pointerPressed_ = false; context.releasePointer(*this);
            keyboardKey_ = key->key;
        }
        if (key->action == KeyAction::Release && keyboardKey_ == key->key) {
            keyboardKey_ = Key::Unknown;
            activate();
        }
        return true;
    }
    return false;
}
void Button::draw(Renderer& renderer) const {
    if (!visible()) return;
    const auto s = style();
    const auto color = !enabled() ? s.disabled : pressed() ? s.pressed : hovered() ? s.hovered : s.normal;
    renderer.drawRoundedRect(bounds(), s.cornerRadius, color);
    if (focused()) renderer.drawBorder(bounds(), theme().focusWidth, s.focus, s.cornerRadius);
    drawText(renderer, enabled() ? textColor() : s.disabledText);
}
} // namespace gui
