#include "gui/Panel.hpp"

#include "gui/Renderer.hpp"

namespace gui {

Panel::Panel(const Rect& bounds) noexcept
    : Widget{bounds} {}

void Panel::setBackgroundColor(const Color& color) noexcept {
    backgroundColor_ = color;
}

const Color& Panel::backgroundColor() const noexcept {
    return backgroundColor_;
}

void Panel::draw(Renderer& renderer) const {
    if (!visible()) {
        return;
    }

    renderer.drawRect(bounds(), backgroundColor_);
}

} // namespace gui
