#include "gui/Label.hpp"
#include "gui/Renderer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace gui {
Label::Label(std::string text, std::shared_ptr<Font> font) : text_{std::move(text)} {
    setFont(std::move(font));
}
void Label::setText(std::string text) {
    const auto metrics = font_ ? font_->measureText(text, fontSize_) : TextMetrics{};
    text_ = std::move(text); metrics_ = metrics; invalidateLayout();
}
void Label::setFont(std::shared_ptr<Font> font) {
    const auto metrics = font ? font->measureText(text_, fontSize_) : TextMetrics{};
    font_ = std::move(font); metrics_ = metrics; invalidateLayout();
}
void Label::setFontSize(float size) {
    if (!std::isfinite(size) || size <= 0 || size > 4096) throw std::invalid_argument("Font size must be in (0, 4096]");
    const auto metrics = font_ ? font_->measureText(text_, size) : TextMetrics{};
    fontSize_ = size; metrics_ = metrics; invalidateLayout();
}
Size Label::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    return {std::max(preferred.width, metrics_.width), std::max(preferred.height, metrics_.height)};
}
void Label::drawText(Renderer& renderer, Color color) const {
    if (!font_ || text_.empty() || bounds().empty()) return;
    const auto& b = bounds();
    float x = b.x, y = b.y;
    if (horizontal_ == HorizontalAlign::Center) x += (b.width - metrics_.width) / 2;
    else if (horizontal_ == HorizontalAlign::Right) x += b.width - metrics_.width;
    if (vertical_ == VerticalAlign::Center) y += (b.height - metrics_.height) / 2;
    else if (vertical_ == VerticalAlign::Bottom) y += b.height - metrics_.height;
    renderer.pushClip(b);
    try { renderer.drawText(font_, text_, {x, y}, fontSize_, color); }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
void Label::draw(Renderer& renderer) const { if (visible()) drawText(renderer, textColor()); }
} // namespace gui
