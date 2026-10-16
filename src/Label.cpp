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
    text_ = std::move(text); metrics_ = metrics; wrapped_.width = -1; invalidateLayout();
}
void Label::setFont(std::shared_ptr<Font> font) {
    const auto metrics = font ? font->measureText(text_, fontSize_) : TextMetrics{};
    font_ = std::move(font); metrics_ = metrics; wrapped_.width = -1; invalidateLayout();
}
void Label::setFontSize(float size) {
    if (!std::isfinite(size) || size <= 0 || size > 4096) throw std::invalid_argument("Font size must be in (0, 4096]");
    const auto metrics = font_ ? font_->measureText(text_, size) : TextMetrics{};
    fontSize_ = size; metrics_ = metrics; wrapped_.width = -1; invalidateLayout();
}
void Label::setWrap(bool wrap) { wrap_ = wrap; wrapped_.width = -1; invalidateLayout(); }
const Label::Wrapped& Label::wrapped(float width) const {
    if (wrapped_.width != width) {
        auto text = font_->wrapText(text_, fontSize_, width);
        const auto metrics = font_->measureText(text, fontSize_);
        wrapped_ = {width, std::move(text), metrics};
    }
    return wrapped_;
}
float Label::heightForWidth(float width) const {
    if (!wrap_ || !font_) return preferredSize().height;
    return std::max(Widget::preferredSize().height, wrapped(width).metrics.height);
}
Size Label::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    return {std::max(preferred.width, metrics_.width), std::max(preferred.height, metrics_.height)};
}
void Label::drawText(Renderer& renderer, Color color, const Rect& area) const {
    if (!font_ || text_.empty() || area.empty()) return;
    const auto& b = area;
    const auto& text = wrap_ ? wrapped(b.width).text : text_;
    const auto& metrics = wrap_ ? wrapped(b.width).metrics : metrics_;
    float y = b.y;
    if (vertical_ == VerticalAlign::Center) y += (b.height - metrics.height) / 2;
    else if (vertical_ == VerticalAlign::Bottom) y += b.height - metrics.height;
    const auto x = [&](float width) {
        return horizontal_ == HorizontalAlign::Center ? b.x + (b.width - width) / 2
            : horizontal_ == HorizontalAlign::Right ? b.x + b.width - width : b.x;
    };
    renderer.pushClip(b);
    try {
        // Center and right alignment apply to each line, not the block.
        if (horizontal_ == HorizontalAlign::Left || text.find('\n') == std::string::npos)
            renderer.drawText(font_, text, {x(metrics.width), y}, fontSize_, color);
        else for (std::size_t start = 0; start <= text.size(); y += metrics.lineHeight) {
            const auto end = std::min(text.find('\n', start), text.size());
            const std::string_view line{text.data() + start, end - start};
            renderer.drawText(font_, line, {x(font_->measureText(line, fontSize_).width), y}, fontSize_, color);
            start = end + 1;
        }
    }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
void Label::draw(Renderer& renderer) const { if (visible()) drawText(renderer, textColor()); }
} // namespace gui
