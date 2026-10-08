#pragma once

#include "gui/Label.hpp"
#include "gui/Input.hpp"
#include <functional>
#include <utility>

namespace gui {
struct ButtonStyle {
    Color normal{0.20f, 0.32f, 0.50f, 1};
    Color hovered{0.26f, 0.43f, 0.66f, 1};
    Color pressed{0.12f, 0.25f, 0.42f, 1};
    Color disabled{0.22f, 0.24f, 0.28f, 1};
    Color disabledText{0.48f, 0.50f, 0.54f, 1};
    Color focus{0.60f, 0.80f, 1.0f, 1};
};
class Button : public Label {
public:
    Button();
    Button(const Button& other);
    Button& operator=(const Button& other);
    explicit Button(std::string text, std::shared_ptr<Font> font = nullptr);
    void setOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }
    void setStyle(ButtonStyle style) noexcept { style_ = style; }
    const ButtonStyle& style() const noexcept { return style_; }
    bool pressed() const noexcept { return keyboardKey_ != Key::Unknown || (pointerPressed_ && hovered()); }
    Size preferredSize() const noexcept override;
    bool onEvent(const Event& event, UIContext& context) override;
    void cancelInteraction() noexcept override;
    void draw(Renderer& renderer) const override;
private:
    void activate();
    ButtonStyle style_{};
    std::function<void()> onClick_;
    bool pointerPressed_{false};
    Key keyboardKey_{Key::Unknown};
};
} // namespace gui
