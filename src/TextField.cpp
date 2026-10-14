#include "gui/TextField.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gui {
namespace {
bool control(char32_t codepoint) noexcept { return codepoint < 0x20 || (codepoint >= 0x7F && codepoint < 0xA0); }
std::string cleanLine(std::string_view text) {
    std::string result;
    for (const auto codepoint : decodeUtf8(text)) if (!control(codepoint)) appendUtf8(result, codepoint);
    return result;
}
} // namespace

TextField::TextField(std::shared_ptr<Font> font, std::string text) : font_{std::move(font)} {
    setFocusable(true);
    setMinimumSize({40, 24});
    setText(text);
}
void TextField::setText(std::string_view text) {
    text_ = cleanLine(text);
    caret_ = text_.size();
    refresh();
}
void TextField::setFont(std::shared_ptr<Font> font) { font_ = std::move(font); refresh(); }
void TextField::setFontSize(float size) {
    if (!std::isfinite(size) || size <= 0 || size > 4096) throw std::invalid_argument("Font size must be in (0, 4096]");
    fontSize_ = size; refresh();
}
void TextField::refresh() {
    std::vector<std::size_t> boundaries{0};
    for (std::size_t i = 0; i < text_.size();) boundaries.push_back(i = nextUtf8Boundary(text_, i));
    auto offsets = font_ ? font_->caretOffsets(text_, fontSize_) : std::vector<float>(boundaries.size(), 0);
    line_ = font_ ? font_->measureText("", fontSize_) : TextMetrics{};
    boundaries_ = std::move(boundaries); offsets_ = std::move(offsets);
    invalidateLayout();
}
void TextField::setCaret(std::size_t offset) noexcept {
    offset = std::min(offset, text_.size());
    caret_ = offset == text_.size() ? offset : previousUtf8Boundary(text_, nextUtf8Boundary(text_, offset));
}
Size TextField::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    return {std::max(preferred.width, 160.0f), std::max(preferred.height, lineHeight() + 2 * paddingY)};
}
Rect TextField::textArea() const noexcept {
    const auto& b = bounds();
    return {b.x + paddingX, b.y, nonnegative(b.width - 2 * paddingX), b.height};
}
float TextField::caretX(std::size_t offset) const noexcept {
    const auto it = std::lower_bound(boundaries_.begin(), boundaries_.end(), offset);
    const auto index = static_cast<std::size_t>(it - boundaries_.begin());
    return index < offsets_.size() ? offsets_[index] : offsets_.back();
}
std::size_t TextField::offsetAt(double x) const noexcept {
    const auto local = static_cast<float>(x - textArea().x) + scroll_;
    const auto it = std::lower_bound(offsets_.begin(), offsets_.end(), local);
    if (it == offsets_.begin()) return boundaries_.front();
    if (it == offsets_.end()) return boundaries_.back();
    const auto index = static_cast<std::size_t>(it - offsets_.begin());
    return local - offsets_[index - 1] <= offsets_[index] - local ? boundaries_[index - 1] : boundaries_[index];
}
void TextField::keepCaretVisible() const noexcept {
    const float width = textArea().width - caretWidth, x = caretX(caret_);
    if (x - scroll_ > width) scroll_ = x - width;
    if (x < scroll_) scroll_ = x;
    scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, offsets_.back() - width));
}
void TextField::replace(std::size_t from, std::size_t to, std::string_view text) {
    if (from == to && text.empty()) return;
    text_.replace(from, to - from, text);
    caret_ = from + text.size();
    refresh();
    keepCaretVisible();
    const auto callback = onChange_;
    if (callback) callback(text_);
}
bool TextField::onEvent(const Event& event, UIContext&) {
    if (!enabled()) return false;
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && mouse->button == MouseButton::Left
        && mouse->action == ButtonAction::Press && hitTest(mouse->x, mouse->y)) {
        keepCaretVisible();
        caret_ = offsetAt(mouse->x);
        return true;
    }
    if (!focused()) return false;
    if (const auto* input = event.getIf<TextInputEvent>()) {
        if (!control(input->codepoint)) {
            std::string encoded;
            appendUtf8(encoded, input->codepoint);
            replace(caret_, caret_, encoded);
        }
        return true;
    }
    const auto* key = event.getIf<KeyEvent>();
    if (!key || key->mods.control || key->mods.alt || key->mods.super) return false;
    switch (key->key) {
        case Key::Escape: case Key::Up: case Key::Down: case Key::PageUp: case Key::PageDown: return false;
        default: break;
    }
    if (key->action == KeyAction::Release) return true;
    switch (key->key) {
        case Key::Left: caret_ = previousUtf8Boundary(text_, caret_); break;
        case Key::Right: caret_ = nextUtf8Boundary(text_, caret_); break;
        case Key::Home: caret_ = 0; break;
        case Key::End: caret_ = text_.size(); break;
        case Key::Backspace: replace(previousUtf8Boundary(text_, caret_), caret_, {}); break;
        case Key::Delete: replace(caret_, nextUtf8Boundary(text_, caret_), {}); break;
        case Key::Enter: case Key::KeypadEnter:
            if (key->action == KeyAction::Press) { const auto callback = onSubmit_; if (callback) callback(text_); }
            break;
        default: break;
    }
    keepCaretVisible();
    return true;
}
void TextField::draw(Renderer& renderer) const {
    if (!visible() || bounds().empty()) return;
    const auto& t = theme();
    const auto& b = bounds();
    renderer.drawRoundedRect(b, t.cornerRadius, enabled() ? t.track : t.disabled);
    renderer.drawBorder(b, focused() ? t.focusWidth : 1, focused() ? t.focus : t.border, t.cornerRadius);
    const auto area = textArea();
    if (!font_ || area.empty()) return;
    keepCaretVisible();
    const float y = area.y + (area.height - lineHeight()) / 2;
    renderer.pushClip(area);
    try {
        if (text_.empty()) {
            if (!placeholder_.empty()) renderer.drawText(font_, placeholder_, {area.x, y}, fontSize_, t.mutedText);
        }
        else renderer.drawText(font_, text_, {area.x - scroll_, y}, fontSize_, enabled() ? t.text : t.disabledText);
        if (focused()) renderer.drawRect({area.x + caretX(caret_) - scroll_, y, caretWidth, lineHeight()}, t.text);
    }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
} // namespace gui
