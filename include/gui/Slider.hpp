#pragma once

#include "gui/Widget.hpp"
#include <functional>
#include <utility>

namespace gui {
// Horizontal value picker. Dragging captures the pointer, so the value keeps
// following the cursor outside the bounds until the button is released.
// Arrow keys step, PageUp/PageDown move ten steps, Home/End jump to the ends.
class Slider : public Widget {
public:
    Slider();
    Slider(float minimum, float maximum, float value = 0);
    Slider(const Slider& other);
    Slider& operator=(const Slider& other);

    // Throws std::invalid_argument unless both are finite and minimum <= maximum.
    void setRange(float minimum, float maximum);
    float minimum() const noexcept { return minimum_; }
    float maximum() const noexcept { return maximum_; }
    // Zero means continuous; keyboard steps are then 1% of the range.
    // Throws std::invalid_argument for negative or nonfinite steps.
    void setStep(float step);
    float step() const noexcept { return step_; }
    // Clamps and snaps to the step. NaN is ignored. Never calls the callback.
    void setValue(float value) noexcept;
    float value() const noexcept { return value_; }
    // Called when user interaction changes the value.
    void setOnChange(std::function<void(float)> callback) { onChange_ = std::move(callback); }
    bool dragging() const noexcept { return dragging_; }

    Size preferredSize() const noexcept override;
    bool onEvent(const Event& event, UIContext& context) override;
    void cancelInteraction() noexcept override { dragging_ = false; }
    void draw(Renderer& renderer) const override;

private:
    static constexpr float thumbRadius = 8;
    float normalize(float value) const noexcept;
    float valueAt(double x) const noexcept;
    float thumbX() const noexcept;
    void change(float value);
    float minimum_{0}, maximum_{1}, step_{0}, value_{0};
    std::function<void(float)> onChange_;
    bool dragging_{false};
};
} // namespace gui
