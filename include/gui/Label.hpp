#pragma once

#include "gui/Color.hpp"
#include "gui/Font.hpp"
#include "gui/Widget.hpp"
#include <memory>
#include <string>

namespace gui {
enum class HorizontalAlign { Left, Center, Right };
enum class VerticalAlign { Top, Center, Bottom };

class Label : public Widget {
public:
    Label() = default;
    explicit Label(std::string text, std::shared_ptr<Font> font = nullptr);
    void setText(std::string text);
    const std::string& text() const noexcept { return text_; }
    void setFont(std::shared_ptr<Font> font);
    const std::shared_ptr<Font>& font() const noexcept { return font_; }
    void setFontSize(float size);
    float fontSize() const noexcept { return fontSize_; }
    void setTextColor(Color color) noexcept { color_ = color; }
    Color textColor() const noexcept { return color_; }
    void setAlignment(HorizontalAlign horizontal, VerticalAlign vertical) noexcept {
        horizontal_ = horizontal; vertical_ = vertical;
    }
    // Wrapped labels break lines to fit their width and report the resulting
    // height through heightForWidth. Their preferred width stays the unwrapped
    // width, so horizontal layouts can shrink them toward their minimum.
    void setWrap(bool wrap);
    bool wrap() const noexcept { return wrap_; }
    Size preferredSize() const noexcept override;
    float heightForWidth(float width) const override;
    void draw(Renderer& renderer) const override;

protected:
    void drawText(Renderer& renderer, Color color) const { drawText(renderer, color, bounds()); }
    // Aligns and clips the text to area instead of the bounds.
    void drawText(Renderer& renderer, Color color, const Rect& area) const;
    const TextMetrics& textMetrics() const noexcept { return metrics_; }

private:
    struct Wrapped { float width{-1}; std::string text; TextMetrics metrics; };
    // Text and metrics as drawn at width, cached per width.
    const Wrapped& wrapped(float width) const;
    mutable Wrapped wrapped_;
    std::string text_;
    std::shared_ptr<Font> font_;
    TextMetrics metrics_{};
    float fontSize_{16};
    Color color_{0.95f, 0.96f, 0.98f, 1};
    HorizontalAlign horizontal_{HorizontalAlign::Left};
    VerticalAlign vertical_{VerticalAlign::Top};
    bool wrap_{false};
};
} // namespace gui
