#include "gui/Checkbox.hpp"
#include "gui/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace gui {
namespace {
Rect expand(const Rect& r, float amount) noexcept {
    return {r.x - amount, r.y - amount, r.width + 2 * amount, r.height + 2 * amount};
}
} // namespace

Checkbox::Checkbox(std::string text, std::shared_ptr<Font> font, bool checked)
    : Button(std::move(text), std::move(font)), checked_{checked} {
    setMinimumSize({});
    setAlignment(HorizontalAlign::Left, VerticalAlign::Center);
}
void Checkbox::activate() {
    checked_ = !checked_;
    const auto callback = onChange_;
    if (callback) callback(checked_);
    Button::activate();
}
Size Checkbox::indicatorSize() const noexcept {
    const float side = std::max(14.0f, std::round(fontSize() * 1.1f));
    return {side, side};
}
Rect Checkbox::indicatorRect() const noexcept {
    const auto& b = bounds();
    const auto size = indicatorSize();
    return {b.x, b.y + (b.height - size.height) / 2, size.width, size.height};
}
Size Checkbox::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize(), indicator = indicatorSize();
    const auto& text = textMetrics();
    const float width = indicator.width + (Label::text().empty() ? 0 : gap + text.width);
    return {std::max(preferred.width, width), std::max({preferred.height, indicator.height, text.height})};
}
void Checkbox::drawIndicator(Renderer& renderer, const Rect& area) const {
    const auto& t = theme();
    constexpr float radius = 4;
    const auto fill = !enabled() ? t.disabled : pressed() ? t.accentPressed : checked_ ? t.fill : t.track;
    renderer.drawRoundedRect(area, radius, fill);
    if (!checked_) renderer.drawBorder(area, 1.5f, hovered() && enabled() ? t.focus : t.border, radius);
    else {
        const auto point = [&](float x, float y) { return Point{area.x + area.width * x, area.y + area.height * y}; };
        const float width = std::max(1.5f, area.width * 0.13f);
        const auto mark = enabled() ? t.background : t.disabledText;
        renderer.drawLine(point(0.25f, 0.52f), point(0.43f, 0.70f), width, mark);
        renderer.drawLine(point(0.43f, 0.70f), point(0.76f, 0.32f), width, mark);
    }
}
void Checkbox::draw(Renderer& renderer) const {
    if (!visible()) return;
    const auto indicator = indicatorRect();
    drawIndicator(renderer, indicator);
    if (focused()) {
        const auto& t = theme();
        renderer.drawBorder(expand(indicator, 3), t.focusWidth, t.focus, std::min(indicator.width, indicator.height) / 2);
    }
    const auto& b = bounds();
    const float textX = indicator.x + indicator.width + gap;
    drawText(renderer, enabled() ? textColor() : theme().disabledText,
             {textX, b.y, std::max(0.0f, b.x + b.width - textX), b.height});
}

Size Switch::indicatorSize() const noexcept {
    const auto box = Checkbox::indicatorSize();
    return {std::round(box.height * 1.8f), box.height};
}
void Switch::drawIndicator(Renderer& renderer, const Rect& area) const {
    const auto& t = theme();
    const float radius = area.height / 2;
    renderer.drawRoundedRect(area, radius, !enabled() ? t.disabled : checked() ? t.fill : t.track);
    if (!checked()) renderer.drawBorder(area, 1.5f, hovered() && enabled() ? t.focus : t.border, radius);
    constexpr float inset = 3;
    const float knob = area.height - 2 * inset;
    const float x = checked() ? area.x + area.width - inset - knob : area.x + inset;
    renderer.drawRoundedRect({x, area.y + inset, knob, knob}, knob / 2,
                             !enabled() ? t.disabledText : pressed() ? t.focus : t.text);
}
} // namespace gui
