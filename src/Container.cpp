#include "gui/Container.hpp"
#include "gui/Renderer.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>
#include <stdexcept>

namespace gui {
Widget& Container::add(std::unique_ptr<Widget> child) {
    if (!child || child->parent_ || child->context_) throw std::invalid_argument("Child must be unattached");
    auto& result = *child;
    auto& destination = context_ && context_->dispatching() ? pendingChildren_ : children_;
    // Allocate before attaching: an allocation failure must not invalidate UI pointers.
    destination.push_back(std::move(child));
    result.parent_ = this;
    result.context_ = context_;
    if (auto* container = dynamic_cast<Container*>(&result)) container->attach(context_);
    invalidateLayout();
    return result;
}
bool Container::remove(Widget& child) {
    if (child.parent_ != this) return false;
    if (context_) context_->unavailable(child);
    if (context_ && context_->dispatching()) child.pendingRemoval_ = true;
    else {
        const auto* pointer = &child;
        const auto erase = [pointer](auto& list) {
            const auto it = std::find_if(list.begin(), list.end(), [pointer](const auto& p) { return p.get() == pointer; });
            if (it != list.end()) list.erase(it);
        };
        erase(children_); erase(pendingChildren_);
    }
    invalidateLayout();
    return true;
}
void Container::attach(UIContext* context) noexcept {
    context_ = context;
    const auto attachChildren = [context](auto& list) {
        for (auto& child : list) {
            child->context_ = context;
            if (auto* container = dynamic_cast<Container*>(child.get())) container->attach(context);
        }
    };
    attachChildren(children_); attachChildren(pendingChildren_);
}
void Container::commitMutations() {
    const auto removed = [](const auto& child) { return child->pendingRemoval_; };
    std::erase_if(children_, removed);
    std::erase_if(pendingChildren_, removed);
    children_.reserve(children_.size() + pendingChildren_.size());
    for (auto& child : pendingChildren_) children_.push_back(std::move(child));
    pendingChildren_.clear();
    for (auto& child : children_) {
        if (auto* container = dynamic_cast<Container*>(child.get())) container->commitMutations();
    }
}
void Container::setLayout(Layout layout) noexcept { layout_ = layout; invalidateLayout(); }
void Container::setPadding(Insets padding) noexcept {
    padding_ = {nonnegative(padding.left), nonnegative(padding.top),
                nonnegative(padding.right), nonnegative(padding.bottom)};
    invalidateLayout();
}
void Container::setSpacing(float spacing) noexcept { spacing_ = nonnegative(spacing); invalidateLayout(); }
Rect Container::contentBounds() const noexcept {
    const auto& b = bounds();
    return {b.x + padding_.left, b.y + padding_.top,
            nonnegative(b.width - padding_.left - padding_.right),
            nonnegative(b.height - padding_.top - padding_.bottom)};
}
Size Container::preferredSize() const noexcept {
    auto result = Widget::preferredSize();
    if (layout_ == Layout::Absolute) return result;
    float main = 0, cross = 0;
    std::size_t count = 0;
    for (const auto& child : children_) {
        if (!child->visible() || child->pendingRemoval_) continue;
        const auto preferred = child->preferredSize(), minimum = child->minimumSize();
        const auto width = std::max(preferred.width, minimum.width);
        const auto height = std::max(preferred.height, minimum.height);
        main += layout_ == Layout::Horizontal ? width : height;
        cross = std::max(cross, layout_ == Layout::Horizontal ? height : width);
        ++count;
    }
    if (count > 1) main += spacing_ * static_cast<float>(count - 1);
    const auto width = (layout_ == Layout::Horizontal ? main : cross) + padding_.left + padding_.right;
    const auto height = (layout_ == Layout::Horizontal ? cross : main) + padding_.top + padding_.bottom;
    return {std::max(result.width, width), std::max(result.height, height)};
}
void Container::arrange() {
    if (layout_ != Layout::Absolute) {
        std::vector<Widget*> visible;
        std::vector<float> lengths, minimums;
        float total = 0, minTotal = 0, flexTotal = 0;
        const bool horizontal = layout_ == Layout::Horizontal;
        for (auto& child : children_) {
            if (!child->visible() || child->pendingRemoval_) continue;
            visible.push_back(child.get());
            const auto preferred = child->preferredSize(), minimum = child->minimumSize();
            const auto min = horizontal ? minimum.width : minimum.height;
            const auto length = std::max(min, horizontal ? preferred.width : preferred.height);
            lengths.push_back(length); minimums.push_back(min);
            total += length; minTotal += min; flexTotal += child->flex();
        }
        const auto content = contentBounds();
        const float space = nonnegative((horizontal ? content.width : content.height)
            - spacing_ * static_cast<float>(visible.empty() ? 0 : visible.size() - 1));
        const float extra = std::max(0.0f, space - total);
        const float deficit = std::min(std::max(0.0f, total - space), total - minTotal);
        float position = horizontal ? content.x : content.y;
        for (std::size_t i = 0; i < visible.size(); ++i) {
            if (extra > 0 && flexTotal > 0) lengths[i] += extra * visible[i]->flex() / flexTotal;
            if (deficit > 0 && total > minTotal) lengths[i] -= deficit * (lengths[i] - minimums[i]) / (total - minTotal);
            const auto minimum = visible[i]->minimumSize();
            visible[i]->setBounds(horizontal
                ? Rect{position, content.y, lengths[i], std::max(content.height, minimum.height)}
                : Rect{content.x, position, std::max(content.width, minimum.width), lengths[i]});
            position += lengths[i] + spacing_;
        }
    }
    for (auto& child : children_) {
        if (child->visible() && !child->pendingRemoval_) {
            if (auto* container = dynamic_cast<Container*>(child.get())) container->arrange();
        }
    }
}
void Container::draw(Renderer& renderer) const {
    if (!visible()) return;
    Panel::draw(renderer);
    if (clipChildren_) renderer.pushClip(contentBounds());
    try {
        for (const auto& child : children_) {
            if (child->visible() && !child->pendingRemoval_) child->draw(renderer);
        }
    }
    catch (...) { if (clipChildren_) renderer.popClip(); throw; }
    if (clipChildren_) renderer.popClip();
}
} // namespace gui
