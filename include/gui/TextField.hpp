#pragma once

#include "gui/Font.hpp"
#include "gui/Widget.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gui {
struct KeyEvent;
// Single-line UTF-8 text entry. Typing arrives through TextInputEvent; the
// caret is a byte offset that always sits on a code point boundary. The
// selection runs between an anchor and the caret. While focused, the field
// consumes key events without Ctrl/Alt/Super so typing never triggers
// application shortcuts, plus the editing shortcuts it implements; Escape, Up,
// Down, PageUp and PageDown still bubble. Ctrl or Super act as the shortcut
// modifier for A/C/X/V.
class TextField : public Widget {
public:
    TextField() : TextField(nullptr) {}
    explicit TextField(std::shared_ptr<Font> font, std::string text = {});

    // Invalid UTF-8 becomes U+FFFD and control characters, including newlines,
    // are removed. Moves the caret to the end. Never calls the change callback.
    void setText(std::string_view text);
    const std::string& text() const noexcept { return text_; }
    void setPlaceholder(std::string placeholder) { placeholder_ = std::move(placeholder); }
    const std::string& placeholder() const noexcept { return placeholder_; }
    void setFont(std::shared_ptr<Font> font);
    const std::shared_ptr<Font>& font() const noexcept { return font_; }
    // Throws std::invalid_argument outside (0, 4096].
    void setFontSize(float size);
    float fontSize() const noexcept { return fontSize_; }
    std::size_t caret() const noexcept { return caret_; }
    // Clamps to the text, moves back to the nearest code point boundary and
    // clears the selection.
    void setCaret(std::size_t offset) noexcept;
    // Byte range [first, second) of the selection; empty when they are equal.
    std::pair<std::size_t, std::size_t> selection() const noexcept {
        return {std::min(anchor_, caret_), std::max(anchor_, caret_)};
    }
    bool hasSelection() const noexcept { return anchor_ != caret_; }
    std::string selectedText() const;
    // The caret ends at caret; both offsets are clamped and snapped like setCaret.
    void setSelection(std::size_t anchor, std::size_t caret) noexcept;
    void selectAll() noexcept { anchor_ = 0; caret_ = text_.size(); }
    bool dragging() const noexcept { return dragging_; }
    // Called after every user edit with the new text.
    void setOnChange(std::function<void(const std::string&)> callback) { onChange_ = std::move(callback); }
    // Called when Enter is pressed.
    void setOnSubmit(std::function<void(const std::string&)> callback) { onSubmit_ = std::move(callback); }

    Size preferredSize() const noexcept override;
    bool onEvent(const Event& event, UIContext& context) override;
    void cancelInteraction() noexcept override { dragging_ = false; }
    void draw(Renderer& renderer) const override;

protected:
    static constexpr float paddingX = 8, paddingY = 6, caretWidth = 1.5f;
    // Padded area that clips the text.
    Rect textArea() const noexcept;
    // X of a boundary relative to the start of the text.
    float caretX(std::size_t offset) const noexcept;
    // Nearest boundary to a window x coordinate.
    std::size_t offsetAt(double x) const noexcept;
    // Replaces bytes [from, to) with already-cleaned text and notifies.
    void replace(std::size_t from, std::size_t to, std::string_view text);
    // Replaces the selection, or inserts at the caret, and notifies.
    void replaceSelection(std::string_view text);
    // Boundaries of the previous or next word start/end around offset.
    std::size_t previousWord(std::size_t offset) const noexcept;
    std::size_t nextWord(std::size_t offset) const noexcept;
    // Scrolls horizontally so the caret stays inside the text area.
    void keepCaretVisible() const noexcept;
    float lineHeight() const noexcept { return line_.lineHeight > 0 ? line_.lineHeight : fontSize_; }
    float scroll() const noexcept { return scroll_; }

private:
    void refresh();
    std::size_t snap(std::size_t offset) const noexcept;
    bool editKey(const KeyEvent& key, UIContext& context);
    std::string text_, placeholder_;
    std::shared_ptr<Font> font_;
    float fontSize_{16};
    std::size_t caret_{0}, anchor_{0};
    bool dragging_{false};
    std::vector<std::size_t> boundaries_{0};
    std::vector<float> offsets_{0};
    TextMetrics line_{};
    mutable float scroll_{0};
    std::function<void(const std::string&)> onChange_, onSubmit_;
};
} // namespace gui
