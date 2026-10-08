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
    const auto color = !enabled() ? style_.disabled : pressed() ? style_.pressed : hovered() ? style_.hovered : style_.normal;
    renderer.drawRect(bounds(), color);
    if (focused()) {
        const auto& b = bounds();
        constexpr float border = 2;
        renderer.drawRect({b.x, b.y, b.width, std::min(border, b.height)}, style_.focus);
        renderer.drawRect({b.x, b.y + std::max(0.0f, b.height - border), b.width, std::min(border, b.height)}, style_.focus);
        renderer.drawRect({b.x, b.y, std::min(border, b.width), b.height}, style_.focus);
        renderer.drawRect({b.x + std::max(0.0f, b.width - border), b.y, std::min(border, b.width), b.height}, style_.focus);
    }
    drawText(renderer, enabled() ? textColor() : style_.disabledText);
}
} // namespace gui
