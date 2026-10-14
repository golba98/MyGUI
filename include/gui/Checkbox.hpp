#pragma once

#include "gui/Button.hpp"
#include <functional>
#include <string>
#include <utility>

namespace gui {
// A labelled on/off control. Clicking anywhere on it, or Enter/Space while
// focused, toggles the state with the same rules as a Button activation.
class Checkbox : public Button {
public:
    Checkbox() : Checkbox("") {}
    explicit Checkbox(std::string text, std::shared_ptr<Font> font = nullptr, bool checked = false);
    // Programmatic changes never call the change callback.
    void setChecked(bool checked) noexcept { checked_ = checked; }
    bool checked() const noexcept { return checked_; }
    // Called after a user toggle, before any click callback.
    void setOnChange(std::function<void(bool)> callback) { onChange_ = std::move(callback); }
    Size preferredSize() const noexcept override;
    void draw(Renderer& renderer) const override;

protected:
    static constexpr float gap = 8;
    void activate() override;
    virtual Size indicatorSize() const noexcept;
    virtual void drawIndicator(Renderer& renderer, const Rect& area) const;
    Rect indicatorRect() const noexcept;

private:
    std::function<void(bool)> onChange_;
    bool checked_{false};
};

// A Checkbox drawn as a sliding pill.
class Switch : public Checkbox {
public:
    using Checkbox::Checkbox;

protected:
    Size indicatorSize() const noexcept override;
    void drawIndicator(Renderer& renderer, const Rect& area) const override;
};
} // namespace gui
