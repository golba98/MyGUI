#include "gui/Widget.hpp"

namespace gui {

Widget::Widget(const Rect& bounds) noexcept
    : bounds_{bounds} {}

void Widget::setBounds(const Rect& bounds) noexcept {
    bounds_ = bounds;
}

const Rect& Widget::bounds() const noexcept {
    return bounds_;
}

void Widget::setVisible(bool visible) noexcept {
    visible_ = visible;
}

bool Widget::visible() const noexcept {
    return visible_;
}

} // namespace gui
