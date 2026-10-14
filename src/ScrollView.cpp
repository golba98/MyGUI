#include "gui/ScrollView.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>

namespace gui {
namespace {
constexpr float minimumThumb = 24, thumbInset = 2;
} // namespace

ScrollView::ScrollView() {
    content_ = &emplace<Container>();
    content_->setLayout(Layout::Vertical);
    content_->setBackgroundColor({0, 0, 0, 0});
}
Point ScrollView::maxScrollOffset() const noexcept {
    return {horizontal_ ? std::max(0.0f, contentSize_.width - viewport_.width) : 0.0f,
            std::max(0.0f, contentSize_.height - viewport_.height)};
}
void ScrollView::setScrollOffset(Point offset) noexcept {
    const auto limit = maxScrollOffset();
    const Point clamped{std::clamp(nonnegative(offset.x), 0.0f, limit.x), std::clamp(nonnegative(offset.y), 0.0f, limit.y)};
    if (clamped.x == offset_.x && clamped.y == offset_.y) return;
    offset_ = clamped;
    invalidateLayout();
}
void ScrollView::scrollIntoView(const Rect& rect) noexcept {
    auto offset = offset_;
    const auto reveal = [](float& value, float start, float length, float viewStart, float viewLength) {
        // Align the leading edge when the rect is larger than the viewport.
        if (start < viewStart || length > viewLength) value -= viewStart - start;
        else if (start + length > viewStart + viewLength) value += start + length - (viewStart + viewLength);
    };
    reveal(offset.y, rect.y, rect.height, viewport_.y, viewport_.height);
    if (horizontal_) reveal(offset.x, rect.x, rect.width, viewport_.x, viewport_.width);
    setScrollOffset(offset);
}
void ScrollView::arrange() {
    const auto area = contentBounds();
    const auto preferred = content_->preferredSize(), minimum = content_->minimumSize();
    // The content height depends on the width it gets, which depends on whether
    // a vertical bar is shown.
    const auto wantedAt = [&](float viewportWidth) {
        const float width = horizontal_ ? std::max({viewportWidth, preferred.width, minimum.width}) : viewportWidth;
        return Size{std::max(preferred.width, minimum.width), std::max(minimum.height, content_->heightForWidth(width))};
    };
    // A bar on one axis shrinks the other axis and can make a second bar necessary.
    bool vertical = false, horizontal = false;
    Size wanted = wantedAt(area.width);
    for (int pass = 0; pass < 2; ++pass) {
        const float width = area.width - (vertical ? scrollbarWidth : 0);
        const float height = area.height - (horizontal ? scrollbarWidth : 0);
        wanted = wantedAt(nonnegative(width));
        vertical = wanted.height > height;
        horizontal = horizontal_ && wanted.width > width;
    }
    wanted = wantedAt(nonnegative(area.width - (vertical ? scrollbarWidth : 0)));
    viewport_ = {area.x, area.y, nonnegative(area.width - (vertical ? scrollbarWidth : 0)),
                 nonnegative(area.height - (horizontal ? scrollbarWidth : 0))};
    contentSize_ = {horizontal_ ? std::max(viewport_.width, wanted.width) : viewport_.width,
                    std::max(viewport_.height, wanted.height)};
    const auto limit = maxScrollOffset();
    offset_ = {std::min(offset_.x, limit.x), std::min(offset_.y, limit.y)};
    content_->setBounds({viewport_.x - offset_.x, viewport_.y - offset_.y, contentSize_.width, contentSize_.height});
    content_->arrange();
}
ScrollView::Bar ScrollView::bar(Axis axis) const noexcept {
    const bool vertical = axis == Axis::Vertical;
    const float view = vertical ? viewport_.height : viewport_.width;
    const float total = vertical ? contentSize_.height : contentSize_.width;
    const float limit = vertical ? maxScrollOffset().y : maxScrollOffset().x;
    if (limit <= 0 || view <= 0) return {};
    const Rect track = vertical ? Rect{viewport_.x + viewport_.width, viewport_.y, scrollbarWidth, viewport_.height}
                                : Rect{viewport_.x, viewport_.y + viewport_.height, viewport_.width, scrollbarWidth};
    const float length = std::min(view, std::max(minimumThumb, view * view / total));
    const float position = (view - length) * (vertical ? offset_.y : offset_.x) / limit;
    const Rect thumb = vertical
        ? Rect{track.x + thumbInset, track.y + position, track.width - 2 * thumbInset, length}
        : Rect{track.x + position, track.y + thumbInset, length, track.height - 2 * thumbInset};
    return {track, thumb, true};
}
bool ScrollView::onEvent(const Event& event, UIContext& context) {
    if (const auto* scroll = event.getIf<MouseScrollEvent>()) {
        const auto before = offset_;
        setScrollOffset({offset_.x - static_cast<float>(horizontal_ ? scroll->xOffset : 0) * lineStep,
                         offset_.y - static_cast<float>(scroll->yOffset) * lineStep});
        return offset_.x != before.x || offset_.y != before.y;
    }
    if (const auto* key = event.getIf<KeyEvent>(); key && key->action != KeyAction::Release
        && (key->key == Key::PageUp || key->key == Key::PageDown)) {
        const auto before = offset_.y;
        setScrollOffset({offset_.x, offset_.y + (key->key == Key::PageUp ? -1 : 1) * viewport_.height});
        return offset_.y != before;
    }
    if (const auto* mouse = event.getIf<MouseButtonEvent>(); mouse && mouse->button == MouseButton::Left) {
        if (mouse->action == ButtonAction::Release) {
            if (drag_ == Axis::None) return false;
            drag_ = Axis::None;
            context.releasePointer(*this);
            return true;
        }
        for (const auto axis : {Axis::Vertical, Axis::Horizontal}) {
            const auto b = bar(axis);
            if (!b.visible || !b.track.contains(mouse->x, mouse->y)) continue;
            const bool vertical = axis == Axis::Vertical;
            const float pointer = static_cast<float>(vertical ? mouse->y : mouse->x);
            const float start = vertical ? b.thumb.y : b.thumb.x, length = vertical ? b.thumb.height : b.thumb.width;
            if (pointer >= start && pointer < start + length) {
                if (context.capturePointer(*this)) { drag_ = axis; grab_ = pointer - start; }
            }
            else {
                const float page = (vertical ? viewport_.height : viewport_.width) * (pointer < start ? -1.0f : 1.0f);
                setScrollOffset(vertical ? Point{offset_.x, offset_.y + page} : Point{offset_.x + page, offset_.y});
            }
            return true;
        }
        return false;
    }
    if (const auto* move = event.getIf<MouseMoveEvent>(); move && drag_ != Axis::None) {
        const bool vertical = drag_ == Axis::Vertical;
        const auto b = bar(drag_);
        const float view = vertical ? viewport_.height : viewport_.width;
        const float length = vertical ? b.thumb.height : b.thumb.width;
        const float travel = view - length, limit = vertical ? maxScrollOffset().y : maxScrollOffset().x;
        const float pointer = static_cast<float>(vertical ? move->y : move->x) - grab_;
        const float trackStart = vertical ? viewport_.y : viewport_.x;
        const float value = travel > 0 ? (pointer - trackStart) / travel * limit : 0;
        setScrollOffset(vertical ? Point{offset_.x, value} : Point{value, offset_.y});
        return true;
    }
    return false;
}
void ScrollView::draw(Renderer& renderer) const {
    if (!visible()) return;
    Container::draw(renderer);
    const auto& t = theme();
    for (const auto axis : {Axis::Vertical, Axis::Horizontal}) {
        const auto b = bar(axis);
        if (!b.visible) continue;
        renderer.drawRect(b.track, {t.track.r, t.track.g, t.track.b, 0.5f});
        const bool active = drag_ == axis || (hovered() && drag_ == Axis::None);
        renderer.drawRoundedRect(b.thumb, std::min(b.thumb.width, b.thumb.height) / 2, active ? t.mutedText : t.border);
    }
}
} // namespace gui
