#include "gui/UIContext.hpp"
#include "gui/Renderer.hpp"

#include <algorithm>
#include <stdexcept>

namespace gui {
UIContext::UIContext() : UIContext(std::make_unique<Container>()) {}
UIContext::UIContext(std::unique_ptr<Container> root) : root_{std::move(root)} {
    if (!root_ || root_->parent() || root_->context_) throw std::invalid_argument("UI root must be unattached");
    root_->attach(this);
}
UIContext::~UIContext() { root_->attach(nullptr); }
bool UIContext::belongsTo(const Widget* widget, const Widget& ancestor) const noexcept {
    for (; widget; widget = widget->parent_) if (widget == &ancestor) return true;
    return false;
}
bool UIContext::available(const Widget* widget) const noexcept {
    if (!widget || widget->context_ != this) return false;
    for (; widget; widget = widget->parent_) {
        if (!widget->visible_ || !widget->enabled_ || widget->pendingRemoval_) return false;
    }
    return true;
}
void UIContext::unavailable(Widget& widget) noexcept {
    if (belongsTo(hovered_, widget)) { hovered_->hovered_ = false; hovered_ = nullptr; }
    if (belongsTo(focused_, widget)) {
        focused_->focused_ = false; focused_->cancelInteraction(); focused_ = nullptr;
    }
    if (belongsTo(captured_, widget)) { captured_->cancelInteraction(); captured_ = nullptr; }
}
bool UIContext::requestFocus(Widget* widget) noexcept {
    if (widget && (!windowFocused_ || !available(widget) || !widget->focusable_)) return false;
    if (focused_ != widget) {
        if (focused_) {
            if (captured_ == focused_) captured_ = nullptr;
            focused_->focused_ = false; focused_->cancelInteraction();
        }
        focused_ = widget;
        if (focused_) focused_->focused_ = true;
    }
    return true;
}
bool UIContext::capturePointer(Widget& widget) noexcept {
    if (!windowFocused_ || !available(&widget)) return false;
    if (captured_ && captured_ != &widget) captured_->cancelInteraction();
    captured_ = &widget;
    return true;
}
void UIContext::releasePointer(Widget& widget) noexcept { if (captured_ == &widget) captured_ = nullptr; }
void UIContext::layout(const Viewport& viewport) {
    if (viewport_.logicalWidth != viewport.logicalWidth || viewport_.logicalHeight != viewport.logicalHeight) dirty_ = true;
    viewport_ = viewport;
    ensureLayout();
    updateHover();
}
void UIContext::ensureLayout() {
    if (!dirty_) return;
    root_->setBounds({0, 0, static_cast<float>(std::max(0, viewport_.logicalWidth)),
                     static_cast<float>(std::max(0, viewport_.logicalHeight))});
    root_->arrange();
    dirty_ = false;
}
Widget* UIContext::targetAt(Widget& widget, double x, double y, Rect clip) {
    if (!widget.visible_ || widget.pendingRemoval_ || !clip.contains(x, y)) return nullptr;
    if (auto* container = dynamic_cast<Container*>(&widget)) {
        const auto childClip = container->clipChildren() ? intersect(clip, container->contentBounds()) : clip;
        for (auto it = container->children_.rbegin(); it != container->children_.rend(); ++it) {
            if (auto* result = targetAt(**it, x, y, childClip)) return result;
        }
    }
    return widget.hitTest(x, y) ? &widget : nullptr;
}
void UIContext::updateHover() {
    Widget* target = cursorInside_ && windowFocused_
        ? targetAt(*root_, mouseX_, mouseY_, root_->bounds()) : nullptr;
    if (!available(target)) target = nullptr;
    if (hovered_ != target) {
        if (hovered_) hovered_->hovered_ = false;
        hovered_ = target;
        if (hovered_) hovered_->hovered_ = true;
    }
}
void UIContext::focusOrder(Widget& widget, std::vector<Widget*>& order) {
    if (!available(&widget)) return;
    if (widget.focusable_) order.push_back(&widget);
    if (auto* container = dynamic_cast<Container*>(&widget)) {
        for (auto& child : container->children_) focusOrder(*child, order);
    }
}
void UIContext::draw(Renderer& renderer) { ensureLayout(); updateHover(); root_->draw(renderer); }
bool UIContext::handleEvent(const Event& event) {
    if (dispatching_) throw std::logic_error("Recursive UI event dispatch is not allowed");
    ensureLayout();
    if (const auto* focus = event.getIf<WindowFocusEvent>()) {
        windowFocused_ = focus->focused;
        if (!windowFocused_) unavailable(*root_);
        updateHover();
        return false;
    }
    if (const auto* enter = event.getIf<CursorEnterEvent>()) {
        cursorInside_ = enter->entered; updateHover(); return false;
    }
    if (const auto* resize = event.getIf<WindowResizeEvent>()) {
        layout({resize->logicalWidth, resize->logicalHeight, resize->width, resize->height}); return false;
    }
    bool pointer = false;
    if (const auto* move = event.getIf<MouseMoveEvent>()) { mouseX_ = move->x; mouseY_ = move->y; pointer = true; }
    if (const auto* button = event.getIf<MouseButtonEvent>()) { mouseX_ = button->x; mouseY_ = button->y; pointer = true; }
    if (const auto* scroll = event.getIf<MouseScrollEvent>()) { mouseX_ = scroll->x; mouseY_ = scroll->y; pointer = true; }
    updateHover();
    if (!windowFocused_) return false;
    if (const auto* key = event.getIf<KeyEvent>(); key && key->key == Key::Tab && key->action == KeyAction::Press) {
        std::vector<Widget*> order;
        focusOrder(*root_, order);
        if (order.empty()) return false;
        const auto it = std::find(order.begin(), order.end(), focused_);
        std::size_t index = it == order.end() ? (key->mods.shift ? order.size() - 1 : 0)
            : (static_cast<std::size_t>(it - order.begin()) + (key->mods.shift ? order.size() - 1 : 1)) % order.size();
        requestFocus(order[index]);
        return true;
    }
    Widget* target = pointer ? (captured_ ? captured_ : (cursorInside_ ? targetAt(*root_, mouseX_, mouseY_, root_->bounds()) : nullptr)) : focused_;
    if (!available(target)) return target != nullptr; // Disabled targets block underlying widgets.
    if (const auto* button = event.getIf<MouseButtonEvent>(); button && button->action == ButtonAction::Press && button->button == MouseButton::Left) {
        Widget* focus = target;
        while (focus && !focus->focusable_) focus = focus->parent_;
        requestFocus(focus);
    }
    std::vector<Widget*> route;
    for (auto* current = target; current; current = current->parent_) route.push_back(current);
    dispatching_ = true;
    bool handled = false;
    try {
        for (auto* current : route) {
            if (!available(current)) break;
            if (current->onEvent(event, *this)) { handled = true; break; }
        }
    }
    catch (...) {
        dispatching_ = false;
        root_->commitMutations();
        dirty_ = true;
        throw;
    }
    dispatching_ = false;
    root_->commitMutations();
    ensureLayout(); updateHover();
    return handled;
}
} // namespace gui
