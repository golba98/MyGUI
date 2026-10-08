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
    Size preferredSize() const noexcept override;
    void draw(Renderer& renderer) const override;

protected:
    void drawText(Renderer& renderer, Color color) const;

private:
    std::string text_;
    std::shared_ptr<Font> font_;
    TextMetrics metrics_{};
    float fontSize_{16};
    Color color_{0.95f, 0.96f, 0.98f, 1};
    HorizontalAlign horizontal_{HorizontalAlign::Left};
    VerticalAlign vertical_{VerticalAlign::Top};
};
} // namespace gui
