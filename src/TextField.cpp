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
// Words are separated by spaces; punctuation stays part of a word.
bool space(std::string_view text, std::size_t offset) noexcept {
    const auto byte = static_cast<unsigned char>(text[offset]);
    return byte == ' ' || text.substr(offset, 2) == "\xC2\xA0" || text.substr(offset, 3) == "\xE3\x80\x80";
}
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
    anchor_ = caret_ = text_.size();
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
std::size_t TextField::snap(std::size_t offset) const noexcept {
    offset = std::min(offset, text_.size());
    return offset == text_.size() ? offset : previousUtf8Boundary(text_, nextUtf8Boundary(text_, offset));
}
void TextField::setCaret(std::size_t offset) noexcept { anchor_ = caret_ = snap(offset); }
void TextField::setSelection(std::size_t anchor, std::size_t caret) noexcept { anchor_ = snap(anchor); caret_ = snap(caret); }
std::string TextField::selectedText() const {
    const auto [first, last] = selection();
    return text_.substr(first, last - first);
}
std::size_t TextField::previousWord(std::size_t offset) const noexcept {
    while (offset > 0 && space(text_, previousUtf8Boundary(text_, offset))) offset = previousUtf8Boundary(text_, offset);
    while (offset > 0 && !space(text_, previousUtf8Boundary(text_, offset))) offset = previousUtf8Boundary(text_, offset);
    return offset;
}
std::size_t TextField::nextWord(std::size_t offset) const noexcept {
    while (offset < text_.size() && space(text_, offset)) offset = nextUtf8Boundary(text_, offset);
    while (offset < text_.size() && !space(text_, offset)) offset = nextUtf8Boundary(text_, offset);
    return offset;
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
    anchor_ = caret_ = from + text.size();
    refresh();
    keepCaretVisible();
    const auto callback = onChange_;
    if (callback) callback(text_);
}
void TextField::replaceSelection(std::string_view text) {
    const auto [first, last] = selection();
    replace(first, last, text);
}
bool TextField::onEvent(const Event& event, UIContext& context) {
    if (!enabled()) return false;
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && mouse->button == MouseButton::Left) {
        if (mouse->action == ButtonAction::Press) {
            if (!hitTest(mouse->x, mouse->y)) return false;
            keepCaretVisible();
            caret_ = offsetAt(mouse->x);
            if (!mouse->mods.shift) anchor_ = caret_;
            dragging_ = context.capturePointer(*this);
            return true;
        }
        if (!dragging_) return false;
        dragging_ = false;
        context.releasePointer(*this);
        return true;
    }
    if (const auto* move = event.getIf<MouseMoveEvent>()) {
        if (!dragging_) return false;
        // Map through the current scroll, then scroll toward the caret.
        caret_ = offsetAt(move->x);
        keepCaretVisible();
        return true;
    }
    if (!focused()) return false;
    if (const auto* input = event.getIf<TextInputEvent>()) {
        if (!control(input->codepoint)) {
            std::string encoded;
            appendUtf8(encoded, input->codepoint);
            replaceSelection(encoded);
        }
        return true;
    }
    const auto* key = event.getIf<KeyEvent>();
    return key && editKey(*key, context);
}
bool TextField::editKey(const KeyEvent& key, UIContext& context) {
    const bool shortcut = (key.mods.control || key.mods.super) && !key.mods.alt;
    const bool word = key.mods.control && !key.mods.alt && !key.mods.super;
    const bool plain = !key.mods.control && !key.mods.alt && !key.mods.super;
    bool handled = plain;
    switch (key.key) {
        case Key::Escape: case Key::Up: case Key::Down: case Key::PageUp: case Key::PageDown: return false;
        case Key::A: case Key::C: case Key::X: case Key::V: handled = plain || shortcut; break;
        case Key::Left: case Key::Right: case Key::Backspace: case Key::Delete: handled = plain || word; break;
        default: break;
    }
    if (!handled || key.action == KeyAction::Release) return handled;
    const auto [first, last] = selection();
    // Moves the caret; Shift keeps the anchor to extend the selection.
    const auto moveTo = [&](std::size_t offset) { caret_ = offset; if (!key.mods.shift) anchor_ = caret_; };
    switch (key.key) {
        case Key::Left:
            moveTo(word ? previousWord(caret_) : hasSelection() && !key.mods.shift ? first : previousUtf8Boundary(text_, caret_));
            break;
        case Key::Right:
            moveTo(word ? nextWord(caret_) : hasSelection() && !key.mods.shift ? last : nextUtf8Boundary(text_, caret_));
            break;
        case Key::Home: moveTo(0); break;
        case Key::End: moveTo(text_.size()); break;
        case Key::Backspace:
            if (hasSelection()) replaceSelection({});
            else replace(word ? previousWord(caret_) : previousUtf8Boundary(text_, caret_), caret_, {});
            break;
        case Key::Delete:
            if (hasSelection()) replaceSelection({});
            else replace(caret_, word ? nextWord(caret_) : nextUtf8Boundary(text_, caret_), {});
            break;
        case Key::Enter: case Key::KeypadEnter:
            if (key.action == KeyAction::Press) { const auto callback = onSubmit_; if (callback) callback(text_); }
            break;
        case Key::A: if (shortcut) selectAll(); break;
        case Key::C: case Key::X:
            if (shortcut && hasSelection() && context.clipboard()) {
                context.clipboard()->setText(selectedText());
                if (key.key == Key::X) replaceSelection({});
            }
            break;
        case Key::V:
            if (shortcut && context.clipboard()) {
                const auto pasted = cleanLine(context.clipboard()->text());
                if (!pasted.empty() || hasSelection()) replaceSelection(pasted);
            }
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
        else {
            if (focused() && hasSelection()) {
                const auto [first, last] = selection();
                const float left = area.x + caretX(first) - scroll_;
                renderer.drawRect({left, y, area.x + caretX(last) - scroll_ - left, lineHeight()}, t.selection);
            }
            renderer.drawText(font_, text_, {area.x - scroll_, y}, fontSize_, enabled() ? t.text : t.disabledText);
        }
        if (focused()) renderer.drawRect({area.x + caretX(caret_) - scroll_, y, caretWidth, lineHeight()}, t.text);
    }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
} // namespace gui
