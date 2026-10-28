#include "gui/UIContext.hpp"
#include "gui/Renderer.hpp"

#include <algorithm>
#include <stdexcept>
#include <exception>

namespace gui {
UIContext::UIContext() : UIContext(std::make_unique<Container>()) {}
UIContext::UIContext(std::unique_ptr<Container> root) : root_{std::move(root)} {
    if (!root_ || root_->parent() || root_->context_) throw std::invalid_argument("UI root must be unattached");
    root_->attach(this);
}
UIContext::~UIContext() {
    identity_.reset();
    for (auto& entry : overlays_) entry->tree->attach(nullptr);
    root_->attach(nullptr);
}
UIContext::Overlay* UIContext::activeModal() const noexcept {
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it)
        if ((*it)->modal && !(*it)->closing && (*it)->tree->visible()) return it->get();
    return nullptr;
}
UIContext::Overlay* UIContext::activePopup() const noexcept {
    for (const auto& entry : overlays_) if (entry->popup && !entry->closing) return entry.get();
    return nullptr;
}
void UIContext::suppressHeld() noexcept {
    for (std::size_t i=0;i<heldButtons_.size();++i) suppressedButtons_[i]=suppressedButtons_[i] || heldButtons_[i];
    for (std::size_t i=0;i<heldKeys_.size();++i) suppressedKeys_[i]=suppressedKeys_[i] || heldKeys_[i];
}
void UIContext::cancelPopup() noexcept {
    if (auto* popup=activePopup()) {
        popup->closing=true; popup->owner=nullptr;
        unavailable(*popup->tree);
        suppressHeld(); invalidateLayout();
    }
}
Rect UIContext::visibleAnchor(const Widget& owner) const noexcept {
    auto visible=intersect(owner.bounds(),root_->bounds());
    for (auto* parent=owner.parent();parent;parent=parent->parent())
        if (parent->clipChildren()) visible=intersect(visible,parent->childClip());
    return visible;
}
bool UIContext::placePopup(Overlay& entry) {
    auto* modal=activeModal();
    if (!entry.owner || !available(entry.owner) || !permitted(entry.owner)
        || entry.scope!=(modal ? modal->handle.id_ : 0) || visibleAnchor(*entry.owner).empty()) return false;
    const auto anchor=entry.owner->bounds(), view=root_->bounds();
    const auto wanted=entry.tree->preferredSize();
    const float width=std::min(view.width,std::max(anchor.width,wanted.width));
    const float below=std::max(0.0f,view.height-anchor.y-anchor.height-entry.placement.gap);
    const float above=std::max(0.0f,anchor.y-entry.placement.gap);
    const float desired=std::min(entry.placement.maxHeight,
        std::max(wanted.height,static_cast<Popup&>(*entry.tree).content().heightForWidth(std::max(0.0f,width-18))+8));
    const bool up=desired>below && above>below;
    const float height=std::min(desired,up ? above : below);
    if (width<=0 || height<=0) return false;
    entry.tree->setBounds({std::clamp(anchor.x,0.0f,view.width-width),
        std::clamp(up ? anchor.y-entry.placement.gap-height : anchor.y+anchor.height+entry.placement.gap,0.0f,std::max(0.0f,view.height-height)),width,height});
    entry.tree->arrange();
    return true;
}
OverlayHandle UIContext::openPopup(Widget& owner, std::unique_ptr<Popup> tree, PopupOptions options) {
    if (!tree || tree->parent_ || tree->context_) throw std::invalid_argument("Popup must be unattached");
    ensureLayout();
    if (!windowFocused_ || !available(&owner) || !permitted(&owner) || visibleAnchor(owner).empty()
        || (activePopup() && belongsTo(&owner,*activePopup()->tree))) return {};
    auto entry=std::make_unique<Overlay>();
    entry->popup=true; entry->owner=&owner; entry->placement={nonnegative(options.maxHeight),nonnegative(options.gap)};
    entry->scope=activeModal() ? activeModal()->handle.id_ : 0;
    entry->tree=std::move(tree);
    if (!placePopup(*entry)) return {};
    cancelPopup();
    if (captured_) { captured_->cancelInteraction(); captured_=nullptr; }
    suppressHeld();
    entry->handle.owner_=identity_; entry->handle.id_=nextOverlay_++;
    auto handle=entry->handle;
    overlays_.push_back(std::move(entry)); overlays_.back()->tree->attach(this);
    invalidateLayout();
    if (!dispatching_ && !settling_) settle();
    return handle;
}
bool UIContext::permitted(const Widget* widget) const noexcept {
    const auto* modal = activeModal();
    if (modal && !belongsTo(widget, *modal->tree)) {
        const auto* popup=activePopup();
        if (!popup || popup->scope!=modal->handle.id_ || !belongsTo(widget,*popup->tree)) return false;
    }
    for (const auto& entry : overlays_)
        if (entry->closing && belongsTo(widget, *entry->tree)) return false;
    return true;
}
void UIContext::traversal(std::vector<Widget*>& order) {
    if (auto* modal = activeModal()) focusOrder(*modal->tree, order);
    else {
        focusOrder(*root_, order);
        for (auto& entry : overlays_) if (!entry->closing && !entry->popup) focusOrder(*entry->tree, order);
    }
}
void UIContext::scopeFocus(Widget* preferred) {
    if (preferred && requestFocus(preferred)) { reveal(*preferred); return; }
    std::vector<Widget*> order;
    traversal(order);
    requestFocus(order.empty() ? nullptr : order.front());
    if (focused_) reveal(*focused_);
}
void UIContext::cancelBlocked() noexcept {
    if (hovered_ && !permitted(hovered_)) { hovered_->hovered_ = false; hovered_ = nullptr; }
    if (focused_ && !permitted(focused_)) requestFocus(nullptr);
    if (captured_ && !permitted(captured_)) { captured_->cancelInteraction(); captured_ = nullptr; }
    for (std::size_t i=0; i<heldButtons_.size(); ++i) suppressedButtons_[i] = suppressedButtons_[i] || heldButtons_[i];
    for (std::size_t i=0; i<heldKeys_.size(); ++i) suppressedKeys_[i] = suppressedKeys_[i] || heldKeys_[i];
}
void UIContext::reconcileModal() {
    auto* modal = activeModal();
    const auto id = modal ? modal->handle.id_ : 0;
    if (id == modalId_) return;
    Widget* restore = nullptr;
    for (auto& entry : overlays_) if (entry->handle.id_ == modalId_) restore = entry->savedFocus;
    cancelPopup();
    modalId_ = id;
    cancelBlocked();
    if (modal && dispatching_) modalDuringDispatch_ = true;
    scopeFocus(restore);
    requestRedraw();
}
OverlayHandle UIContext::openOverlay(std::unique_ptr<Container> tree, OverlayOptions options) {
    if (!tree || tree->parent_ || tree->context_) throw std::invalid_argument("Overlay must be unattached");
    auto entry = std::make_unique<Overlay>();
    entry->handle.owner_ = identity_; entry->handle.id_ = nextOverlay_++;
    const auto handle = entry->handle;
    entry->tree = std::move(tree); entry->modal = options.modal;
    entry->savedFocus = focused_;
    auto* attached = entry->tree.get();
    auto position = options.modal ? overlays_.end() : std::find_if(overlays_.begin(), overlays_.end(),
        [](const auto& e) { return e->modal || e->popup; });
    overlays_.insert(position, std::move(entry));
    attached->attach(this);
    invalidateLayout();
    reconcileModal();
    return handle;
}
OverlayHandle UIContext::openDialog(std::unique_ptr<Dialog> dialog,
    std::function<void(Dialog&, const DialogResult&)> completion) {
    auto* pointer = dialog.get();
    const auto handle = openOverlay(std::move(dialog), {true});
    for (auto& entry : overlays_) if (entry->handle.id_ == handle.id_) entry->completion = std::move(completion);
    pointer->close_ = [this, handle](DialogResult result) { return closeEntry(handle, std::move(result)); };
    return handle;
}
bool UIContext::isOverlayOpen(OverlayHandle handle) const noexcept {
    if (handle.owner_.lock() != identity_ || !identity_) return false;
    return std::any_of(overlays_.begin(), overlays_.end(), [&](const auto& e) { return e->handle.id_ == handle.id_ && !e->closing; });
}
bool UIContext::closeOverlay(OverlayHandle handle) { return closeEntry(handle, {}); }
bool UIContext::closeEntry(OverlayHandle handle, DialogResult result) {
    if (!isOverlayOpen(handle)) return false;
    for (auto& entry : overlays_) if (entry->handle.id_ == handle.id_) {
        if (entry->popup) { entry->owner=nullptr; suppressHeld(); }
        entry->closing = true; entry->result = std::move(result);
        unavailable(*entry->tree);
        break;
    }
    invalidateLayout();
    reconcileModal();
    if (!dispatching_ && !settling_) settle();
    return true;
}
void UIContext::settle() {
    if (settling_) return;
    settling_ = true;
    std::exception_ptr failure;
    try {
        for (;;) {
            root_->commitMutations();
            for (auto& entry : overlays_) entry->tree->commitMutations();
            auto it = std::find_if(overlays_.begin(), overlays_.end(), [](const auto& e) { return e->closing; });
            if (it == overlays_.end()) break;
            auto retired = std::move(*it); overlays_.erase(it);
            retired->tree->attach(nullptr);
            if (auto* dialog = dynamic_cast<Dialog*>(retired->tree.get())) {
                dialog->close_ = {};
                // Queue mutations made by application completions, retaining the retired tree.
                dispatching_ = true;
                try { if (retired->completion) retired->completion(*dialog, retired->result); }
                catch (...) { if (!failure) failure = std::current_exception(); }
                dispatching_ = false;
            }
        }
    } catch (...) { if (!failure) failure = std::current_exception(); }
    dispatching_ = false; settling_ = false;
    if (failure) std::rethrow_exception(failure);
}
Widget* UIContext::pointerTarget() {
    const auto clip = root_->bounds();
    if (auto* popup=activePopup()) if (auto* target=targetAt(*popup->tree,mouseX_,mouseY_,clip)) return target;
    if (auto* modal = activeModal()) return targetAt(*modal->tree, mouseX_, mouseY_, clip);
    for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it)
        if (!(*it)->closing) if (auto* target = targetAt(*(*it)->tree, mouseX_, mouseY_, clip)) return target;
    return targetAt(*root_, mouseX_, mouseY_, clip);
}
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
    redraw_ = true;
    if (auto* popup=activePopup(); popup && belongsTo(popup->owner,widget)) cancelPopup();
    for (auto& entry : overlays_) if (belongsTo(entry->savedFocus, widget)) entry->savedFocus = nullptr;
    if (belongsTo(hovered_, widget)) { hovered_->hovered_ = false; hovered_ = nullptr; }
    if (belongsTo(focused_, widget)) {
        focused_->focused_ = false; focused_->cancelInteraction(); focused_ = nullptr;
    }
    if (belongsTo(captured_, widget)) { captured_->cancelInteraction(); captured_ = nullptr; }
}
bool UIContext::requestFocus(Widget* widget) noexcept {
    if (widget && (!windowFocused_ || !available(widget) || !permitted(widget) || !widget->focusable_)) return false;
    if (focused_ != widget) {
        if (auto* popup=activePopup(); popup && widget!=popup->owner) cancelPopup();
        redraw_ = true;
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
    if (!windowFocused_ || !available(&widget) || !permitted(&widget)) return false;
    if (captured_ && captured_ != &widget) captured_->cancelInteraction();
    captured_ = &widget;
    return true;
}
void UIContext::releasePointer(Widget& widget) noexcept { if (captured_ == &widget) captured_ = nullptr; }
void UIContext::layout(const Viewport& viewport) {
    if (viewport_.logicalWidth != viewport.logicalWidth || viewport_.logicalHeight != viewport.logicalHeight) dirty_ = true;
    if (viewport_.framebufferWidth != viewport.framebufferWidth || viewport_.framebufferHeight != viewport.framebufferHeight) requestRedraw();
    viewport_ = viewport;
    reconcileModal();
    ensureLayout();
    updateHover();
}
void UIContext::ensureLayout() {
    if (!dirty_) return;
    root_->setBounds({0, 0, static_cast<float>(std::max(0, viewport_.logicalWidth)),
                     static_cast<float>(std::max(0, viewport_.logicalHeight))});
    root_->arrange();
    for (auto& entry : overlays_) if (!entry->closing && !entry->popup) { entry->tree->setBounds(root_->bounds()); entry->tree->arrange(); }
    if (auto* popup=activePopup(); popup && !placePopup(*popup)) cancelPopup();
    dirty_ = false;
}
Widget* UIContext::targetAt(Widget& widget, double x, double y, Rect clip) {
    if (!widget.visible_ || widget.pendingRemoval_ || !clip.contains(x, y)) return nullptr;
    if (auto* container = dynamic_cast<Container*>(&widget)) {
        const auto childClip = container->clipChildren() ? intersect(clip, container->childClip()) : clip;
        for (auto it = container->children_.rbegin(); it != container->children_.rend(); ++it) {
            if (auto* result = targetAt(**it, x, y, childClip)) return result;
        }
    }
    return widget.hitTest(x, y) ? &widget : nullptr;
}
void UIContext::updateHover() {
    Widget* target = cursorInside_ && windowFocused_
        ? pointerTarget() : nullptr;
    if (!available(target)) target = nullptr;
    if (hovered_ != target) {
        redraw_ = true;
        if (hovered_) hovered_->hovered_ = false;
        hovered_ = target;
        if (hovered_) hovered_->hovered_ = true;
    }
}
void UIContext::focusOrder(Widget& widget, std::vector<Widget*>& order) {
    if (!available(&widget) || !permitted(&widget)) return;
    if (widget.focusable_) order.push_back(&widget);
    if (auto* container = dynamic_cast<Container*>(&widget)) {
        for (auto& child : container->children_) focusOrder(*child, order);
    }
}
void UIContext::reveal(Widget& widget) {
    // Innermost first; each step relayouts so outer views see updated bounds.
    constexpr float margin = 4;
    for (auto* ancestor = widget.parent_; ancestor; ancestor = ancestor->parent_) {
        ensureLayout();
        const auto& b = widget.bounds();
        ancestor->scrollIntoView({b.x - margin, b.y - margin, b.width + 2 * margin, b.height + 2 * margin});
    }
    ensureLayout();
    updateHover();
}
void UIContext::markDrawn() { reconcileModal(); ensureLayout(); updateHover(); redraw_ = false; }
void UIContext::draw(Renderer& renderer) {
    reconcileModal(); ensureLayout(); updateHover();
    redraw_ = false;
    try {
        root_->draw(renderer);
        renderer.pushClip(root_->bounds());
        try { for (const auto& entry : overlays_) if (!entry->closing) entry->tree->draw(renderer); }
        catch (...) { renderer.popClip(); throw; }
        renderer.popClip();
    }
    catch (...) { redraw_ = true; throw; }
}
bool UIContext::handleEvent(const Event& event) {
    if (dispatching_) throw std::logic_error("Recursive UI event dispatch is not allowed");
    reconcileModal(); ensureLayout();
    modalDuringDispatch_ = activeModal() != nullptr;
    if (event.getIf<WindowCloseEvent>()) return false;
    if (const auto* focus = event.getIf<WindowFocusEvent>()) {
        windowFocused_ = focus->focused;
        if (!windowFocused_) {
            cancelPopup();
            requestFocus(nullptr);
            if (captured_) { captured_->cancelInteraction(); captured_ = nullptr; }
            heldButtons_.fill(false); heldKeys_.fill(false);
            suppressedButtons_.fill(false); suppressedKeys_.fill(false);
        } else if (activeModal()) scopeFocus();
        updateHover();
        return false;
    }
    if (const auto* enter = event.getIf<CursorEnterEvent>()) {
        cursorInside_ = enter->entered; updateHover(); return false;
    }
    if (event.getIf<WindowRefreshEvent>()) { redraw_ = true; return false; }
    if (const auto* resize = event.getIf<WindowResizeEvent>()) {
        layout({resize->logicalWidth, resize->logicalHeight, resize->width, resize->height}); return false;
    }
    bool pointer = false;
    if (const auto* move = event.getIf<MouseMoveEvent>()) { mouseX_ = move->x; mouseY_ = move->y; pointer = true; }
    if (const auto* button = event.getIf<MouseButtonEvent>()) { mouseX_ = button->x; mouseY_ = button->y; pointer = true; }
    if (const auto* scroll = event.getIf<MouseScrollEvent>()) { mouseX_ = scroll->x; mouseY_ = scroll->y; pointer = true; }
    updateHover();
    if (const auto* button = event.getIf<MouseButtonEvent>()) {
        const auto i = static_cast<std::size_t>(button->button);
        if (i < heldButtons_.size()) {
            heldButtons_[i] = button->action == ButtonAction::Press;
            if (suppressedButtons_[i]) {
                if (!heldButtons_[i]) suppressedButtons_[i] = false;
                return true;
            }
        }
    }
    if (const auto* key = event.getIf<KeyEvent>()) {
        const auto i = static_cast<std::size_t>(key->key);
        if (i < heldKeys_.size()) {
            heldKeys_[i] = key->action != KeyAction::Release;
            if (suppressedKeys_[i]) {
                if (!heldKeys_[i]) suppressedKeys_[i] = false;
                return true;
            }
        }
    }
    if (!windowFocused_) return activeModal() != nullptr;
    if (auto* popup=activePopup()) {
        if (const auto* button=event.getIf<MouseButtonEvent>(); button && button->action==ButtonAction::Press && !popup->tree->hitTest(button->x,button->y)) {
            cancelPopup(); settle(); updateHover(); return true;
        }
        if (const auto* key=event.getIf<KeyEvent>(); key && key->action==KeyAction::Press) {
            if (key->key==Key::Escape) { cancelPopup(); settle(); updateHover(); return true; }
            if (key->key==Key::Tab) cancelPopup();
        }
    }
    const bool popupInput=activePopup() && (event.getIf<KeyEvent>() || event.getIf<TextInputEvent>());
    if (const auto* key = event.getIf<KeyEvent>(); key && key->key == Key::Tab && key->action == KeyAction::Press) {
        std::vector<Widget*> order;
        traversal(order);
        if (order.empty()) return activeModal() != nullptr;
        const auto it = std::find(order.begin(), order.end(), focused_);
        std::size_t index = it == order.end() ? (key->mods.shift ? order.size() - 1 : 0)
            : (static_cast<std::size_t>(it - order.begin()) + (key->mods.shift ? order.size() - 1 : 1)) % order.size();
        if (requestFocus(order[index])) reveal(*order[index]);
        return true;
    }
    Widget* target = pointer ? (captured_ ? captured_ : (cursorInside_ ? pointerTarget() : nullptr)) : focused_;
    if (!target && activeModal()) target = activeModal()->tree.get();
    if (!available(target) || !permitted(target)) return target != nullptr || popupInput || activeModal(); // Disabled targets block underlying widgets.
    if (const auto* button = event.getIf<MouseButtonEvent>(); button && button->action == ButtonAction::Press && button->button == MouseButton::Left
        && !(activePopup() && belongsTo(target,*activePopup()->tree))) {
        Widget* focus = target;
        while (focus && !focus->focusable_) focus = focus->parent_;
        requestFocus(focus);
    }
    std::vector<Widget*> route;
    for (auto* current = target; current; current = popupInput ? nullptr : current->parent_) route.push_back(current);
    dispatching_ = true;
    bool handled = false;
    try {
        for (auto* current : route) {
            if (!available(current) || !permitted(current)) break;
            if (current->onEvent(event, *this)) { handled = true; break; }
        }
    }
    catch (...) {
        dispatching_ = false;
        const auto failure = std::current_exception();
        try { settle(); } catch (...) {}
        dirty_ = true;
        std::rethrow_exception(failure);
    }
    dispatching_ = false;
    settle();
    if (handled) redraw_ = true;
    ensureLayout(); updateHover();
    return handled || popupInput || modalDuringDispatch_ || activeModal();
}
} // namespace gui
