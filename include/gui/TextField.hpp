#pragma once

#include "gui/Font.hpp"
#include "gui/Widget.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gui {
// Single-line UTF-8 text entry. Typing arrives through TextInputEvent; the
// caret is a byte offset that always sits on a code point boundary. While
// focused, the field consumes key events without Ctrl/Alt/Super so typing
// never triggers application shortcuts; Escape, Up, Down, PageUp and PageDown
// still bubble.
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
    // Clamps to the text and moves back to the nearest code point boundary.
    void setCaret(std::size_t offset) noexcept;
    // Called after every user edit with the new text.
    void setOnChange(std::function<void(const std::string&)> callback) { onChange_ = std::move(callback); }
    // Called when Enter is pressed.
    void setOnSubmit(std::function<void(const std::string&)> callback) { onSubmit_ = std::move(callback); }

    Size preferredSize() const noexcept override;
    bool onEvent(const Event& event, UIContext& context) override;
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
    // Scrolls horizontally so the caret stays inside the text area.
    void keepCaretVisible() const noexcept;
    float lineHeight() const noexcept { return line_.lineHeight > 0 ? line_.lineHeight : fontSize_; }
    float scroll() const noexcept { return scroll_; }

private:
    void refresh();
    std::string text_, placeholder_;
    std::shared_ptr<Font> font_;
    float fontSize_{16};
    std::size_t caret_{0};
    std::vector<std::size_t> boundaries_{0};
    std::vector<float> offsets_{0};
    TextMetrics line_{};
    mutable float scroll_{0};
    std::function<void(const std::string&)> onChange_, onSubmit_;
};
} // namespace gui
