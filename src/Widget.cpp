#include "gui/Widget.hpp"
#include "gui/UIContext.hpp"

namespace gui {
Widget::Widget(const Rect& bounds) noexcept { setBounds(bounds); }
Widget::Widget(const Widget& other) noexcept { *this = other; }
Widget& Widget::operator=(const Widget& other) noexcept {
    if (this != &other) {
        if (context_) context_->unavailable(*this);
        bounds_ = other.bounds_;
        minimumSize_ = other.minimumSize_;
        preferredSize_ = other.preferredSize_;
        flex_ = other.flex_;
        visible_ = other.visible_; enabled_ = other.enabled_; focusable_ = other.focusable_;
        invalidateLayout();
    }
    return *this;
}
void Widget::invalidateLayout() noexcept { if (context_) context_->invalidateLayout(); }
void Widget::setBounds(const Rect& bounds) noexcept {
    bounds_ = {std::isfinite(bounds.x) ? bounds.x : 0, std::isfinite(bounds.y) ? bounds.y : 0,
               nonnegative(bounds.width), nonnegative(bounds.height)};
    invalidateLayout();
}
void Widget::setVisible(bool visible) noexcept {
    visible_ = visible;
    if (!visible && context_) context_->unavailable(*this);
    invalidateLayout();
}
void Widget::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled && context_) context_->unavailable(*this);
}
void Widget::setFocusable(bool focusable) noexcept {
    focusable_ = focusable;
    if (!focusable && context_ && focused_) context_->requestFocus(nullptr);
}
void Widget::setMinimumSize(Size size) noexcept {
    minimumSize_ = {nonnegative(size.width), nonnegative(size.height)};
    invalidateLayout();
}
void Widget::setPreferredSize(Size size) noexcept {
    preferredSize_ = {nonnegative(size.width), nonnegative(size.height)};
    invalidateLayout();
}
void Widget::setFlex(float weight) noexcept { flex_ = nonnegative(weight); invalidateLayout(); }
} // namespace gui
