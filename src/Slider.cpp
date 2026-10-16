#include "gui/Slider.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gui {
Slider::Slider() : Slider(0, 1) {}
Slider::Slider(float minimum, float maximum, float value) {
    setRange(minimum, maximum);
    setValue(value);
    setFocusable(true);
    setMinimumSize({thumbRadius * 2 + 8, thumbRadius * 2});
}
Slider::Slider(const Slider& other)
    : Widget(other), minimum_{other.minimum_}, maximum_{other.maximum_}, step_{other.step_},
      value_{other.value_}, onChange_{other.onChange_} {}
Slider& Slider::operator=(const Slider& other) {
    if (this != &other) {
        auto callback = other.onChange_;
        Widget::operator=(other);
        minimum_ = other.minimum_; maximum_ = other.maximum_; step_ = other.step_; value_ = other.value_;
        onChange_ = std::move(callback); dragging_ = false;
    }
    return *this;
}
void Slider::setRange(float minimum, float maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
        throw std::invalid_argument("Slider range must be finite with minimum <= maximum");
    minimum_ = minimum; maximum_ = maximum; value_ = normalize(value_); requestRedraw();
}
void Slider::setStep(float step) {
    if (!std::isfinite(step) || step < 0) throw std::invalid_argument("Slider step must be finite and nonnegative");
    step_ = step; value_ = normalize(value_); requestRedraw();
}
void Slider::setValue(float value) noexcept {
    if (std::isnan(value)) return;
    value_ = normalize(value);
    requestRedraw();
}
float Slider::normalize(float value) const noexcept {
    if (step_ > 0) value = minimum_ + std::round((value - minimum_) / step_) * step_;
    return std::clamp(value, minimum_, maximum_);
}
void Slider::change(float value) {
    const auto previous = value_;
    setValue(value);
    if (value_ != previous) { const auto callback = onChange_; if (callback) callback(value_); }
}
float Slider::valueAt(double x) const noexcept {
    const auto& b = bounds();
    const double left = b.x + thumbRadius, width = b.width - 2 * thumbRadius;
    const double t = width > 0 ? std::clamp((x - left) / width, 0.0, 1.0) : 0.0;
    return minimum_ + static_cast<float>(t) * (maximum_ - minimum_);
}
float Slider::thumbX() const noexcept {
    const auto& b = bounds();
    const float range = maximum_ - minimum_, t = range > 0 ? (value_ - minimum_) / range : 0;
    return b.x + thumbRadius + t * std::max(0.0f, b.width - 2 * thumbRadius);
}
Size Slider::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    return {std::max(preferred.width, 160.0f), std::max(preferred.height, thumbRadius * 2 + 8)};
}
bool Slider::onEvent(const Event& event, UIContext& context) {
    if (!enabled()) return false;
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && mouse->button == MouseButton::Left) {
        if (mouse->action == ButtonAction::Press) {
            if (!hitTest(mouse->x, mouse->y)) return false;
            dragging_ = context.capturePointer(*this);
            if (dragging_) change(valueAt(mouse->x));
            return dragging_;
        }
        if (!dragging_) return false;
        dragging_ = false;
        context.releasePointer(*this);
        change(valueAt(mouse->x));
        return true;
    }
    if (const auto* move = event.getIf<MouseMoveEvent>()) {
        if (dragging_) change(valueAt(move->x));
        return dragging_;
    }
    if (const auto* key = event.getIf<KeyEvent>(); key && focused() && key->action != KeyAction::Release) {
        const float unit = step_ > 0 ? step_ : (maximum_ - minimum_) / 100;
        switch (key->key) {
            case Key::Left: case Key::Down: change(value_ - unit); return true;
            case Key::Right: case Key::Up: change(value_ + unit); return true;
            case Key::PageDown: change(value_ - unit * 10); return true;
            case Key::PageUp: change(value_ + unit * 10); return true;
            case Key::Home: change(minimum_); return true;
            case Key::End: change(maximum_); return true;
            default: break;
        }
    }
    return false;
}
void Slider::draw(Renderer& renderer) const {
    if (!visible() || bounds().empty()) return;
    const auto& t = theme();
    const auto& b = bounds();
    const float cy = b.y + b.height / 2, left = b.x + thumbRadius, x = thumbX();
    constexpr float track = 4;
    renderer.drawRoundedRect({left, cy - track / 2, std::max(0.0f, b.width - 2 * thumbRadius), track}, track / 2,
                             enabled() ? t.track : t.disabled);
    if (enabled()) renderer.drawRoundedRect({left, cy - track / 2, x - left, track}, track / 2, t.fill);
    const Rect thumb{x - thumbRadius, cy - thumbRadius, thumbRadius * 2, thumbRadius * 2};
    renderer.drawRoundedRect(thumb, thumbRadius, !enabled() ? t.disabledText : dragging_ ? t.focus : t.text);
    if (hovered() && enabled() && !dragging_) renderer.drawBorder(thumb, 2, t.fill, thumbRadius);
    if (focused()) {
        constexpr float ring = 3;
        renderer.drawBorder({thumb.x - ring, thumb.y - ring, thumb.width + 2 * ring, thumb.height + 2 * ring},
                            t.focusWidth, t.focus, thumbRadius + ring);
    }
}
} // namespace gui
