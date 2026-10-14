#pragma once

#include "gui/Widget.hpp"

namespace gui {
// Displays completion in [0, 1]. It takes no input.
class ProgressBar : public Widget {
public:
    ProgressBar() { setMinimumSize({16, 4}); }
    explicit ProgressBar(float value) : ProgressBar() { setValue(value); }
    // Clamps to [0, 1]; NaN becomes 0.
    void setValue(float value) noexcept;
    float value() const noexcept { return value_; }
    Size preferredSize() const noexcept override;
    void draw(Renderer& renderer) const override;

private:
    float value_{0};
};
} // namespace gui
