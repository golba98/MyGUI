#pragma once

#include "gui/Clipboard.hpp"
#include "gui/Dialog.hpp"
#include "gui/Overlay.hpp"
#include "gui/Popup.hpp"
#include <array>
#include "gui/Container.hpp"
#include "gui/Event.hpp"
#include "gui/Theme.hpp"
#include "gui/Viewport.hpp"
#include <memory>
#include <vector>

namespace gui {
// One tree per window; dispatch each event once through its UIContext.
class UIContext {
public:
    UIContext();
    explicit UIContext(std::unique_ptr<Container> root);
    ~UIContext();
    UIContext(const UIContext&) = delete;
    UIContext& operator=(const UIContext&) = delete;
    Container& root() noexcept { return *root_; }
    const Container& root() const noexcept { return *root_; }
    // Widgets read the theme while drawing, so changes apply to the next frame.
    void setTheme(const Theme& theme) noexcept { theme_ = theme; invalidateLayout(); }
    const Theme& theme() const noexcept { return theme_; }
    // Non-owning; the clipboard must outlive its use here. nullptr disables
    // copy and paste in text widgets.
    void setClipboard(Clipboard* clipboard) noexcept { clipboard_ = clipboard; }
    Clipboard* clipboard() const noexcept { return clipboard_; }
    OverlayHandle openOverlay(std::unique_ptr<Container> overlay, OverlayOptions options = {});
    OverlayHandle openDialog(std::unique_ptr<Dialog> dialog,
        std::function<void(Dialog&, const DialogResult&)> completion = {});
    OverlayHandle openPopup(Widget& owner, std::unique_ptr<Popup>, PopupOptions = {});
    bool closeOverlay(OverlayHandle handle);
    bool isOverlayOpen(OverlayHandle handle) const noexcept;
    void layout(const Viewport& viewport);
    void draw(Renderer& renderer);
    bool handleEvent(const Event& event);
    bool requestFocus(Widget* widget) noexcept;
    // Scrolls every enclosing view so the widget is visible. Tab traversal calls this.
    void reveal(Widget& widget);
    bool capturePointer(Widget& widget) noexcept;
    void releasePointer(Widget& widget) noexcept;
    Widget* focusedWidget() const noexcept { return focused_; }
    Widget* capturedWidget() const noexcept { return captured_; }
    Widget* hoveredWidget() const noexcept { return hovered_; }
    bool dispatching() const noexcept { return dispatching_; }
    // True when something visible may have changed since the last draw():
    // layout, hover, focus, handled events, window refresh requests, or a
    // widget property. Applications that wait for events can skip drawing
    // while this is false. Starts true; draw() clears it.
    bool needsRedraw() const noexcept { return redraw_; }
    void requestRedraw() noexcept { redraw_ = true; }
    // Completes pending layout and clears the flag without drawing, for
    // frames drawn another way.
    void markDrawn();

private:
    friend class Widget;
    friend class Container;
    void invalidateLayout() noexcept { dirty_ = true; redraw_ = true; }
    void unavailable(Widget& widget) noexcept;
    bool available(const Widget* widget) const noexcept;
    bool belongsTo(const Widget* widget, const Widget& ancestor) const noexcept;
    Widget* targetAt(Widget& widget, double x, double y, Rect clip);
    void updateHover();
    void ensureLayout();
    void focusOrder(Widget& widget, std::vector<Widget*>& order);
    struct Overlay {
        OverlayHandle handle;
        std::unique_ptr<Container> tree;
        bool modal{}, closing{};
        Widget* savedFocus{};
        bool popup{};
        Widget* owner{};
        std::uint64_t scope{};
        PopupOptions placement;
        DialogResult result;
        std::function<void(Dialog&, const DialogResult&)> completion;
    };
    Overlay* activeModal() const noexcept;
    Overlay* activePopup() const noexcept;
    Rect visibleAnchor(const Widget&) const noexcept;
    bool placePopup(Overlay&);
    void cancelPopup() noexcept;
    void suppressHeld() noexcept;
    bool permitted(const Widget*) const noexcept;
    Widget* pointerTarget();
    void scopeFocus(Widget* preferred = nullptr);
    void reconcileModal();
    void cancelBlocked() noexcept;
    bool closeEntry(OverlayHandle, DialogResult);
    void settle();
    void traversal(std::vector<Widget*>&);
    std::shared_ptr<int> identity_{std::make_shared<int>(0)};
    std::uint64_t nextOverlay_{1}, modalId_{};
    std::vector<std::unique_ptr<Overlay>> overlays_;
    bool settling_{false}, modalDuringDispatch_{false};
    std::array<bool, 8> heldButtons_{}, suppressedButtons_{};
    std::array<bool, static_cast<std::size_t>(Key::Menu)+1> heldKeys_{}, suppressedKeys_{};
    std::unique_ptr<Container> root_;
    Theme theme_{};
    Clipboard* clipboard_{nullptr};
    Viewport viewport_{};
    Widget* hovered_{nullptr};
    Widget* focused_{nullptr};
    Widget* captured_{nullptr};
    double mouseX_{0}, mouseY_{0};
    bool cursorInside_{true}, windowFocused_{true};
    bool dirty_{true}, dispatching_{false}, redraw_{true};
};
} // namespace gui
