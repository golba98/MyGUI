#pragma once

#include "gui/Color.hpp"
#include "gui/Widget.hpp"

namespace gui {

// A rectangle filled with a solid background color.
class Panel : public Widget {
public:
    Panel() = default;
    explicit Panel(const Rect& bounds) noexcept;

    void setBackgroundColor(const Color& color) noexcept;
    const Color& backgroundColor() const noexcept;

    void draw(Renderer& renderer) const override;

private:
    Color backgroundColor_{};
};

} // namespace gui
