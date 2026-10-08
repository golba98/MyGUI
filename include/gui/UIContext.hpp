#pragma once

#include "gui/Container.hpp"
#include "gui/Event.hpp"
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
    void layout(const Viewport& viewport);
    void draw(Renderer& renderer);
    bool handleEvent(const Event& event);
    bool requestFocus(Widget* widget) noexcept;
    bool capturePointer(Widget& widget) noexcept;
    void releasePointer(Widget& widget) noexcept;
    Widget* focusedWidget() const noexcept { return focused_; }
    Widget* capturedWidget() const noexcept { return captured_; }
    Widget* hoveredWidget() const noexcept { return hovered_; }
    bool dispatching() const noexcept { return dispatching_; }

private:
    friend class Widget;
    friend class Container;
    void invalidateLayout() noexcept { dirty_ = true; }
    void unavailable(Widget& widget) noexcept;
    bool available(const Widget* widget) const noexcept;
    bool belongsTo(const Widget* widget, const Widget& ancestor) const noexcept;
    Widget* targetAt(Widget& widget, double x, double y, Rect clip);
    void updateHover();
    void ensureLayout();
    void focusOrder(Widget& widget, std::vector<Widget*>& order);
    std::unique_ptr<Container> root_;
    Viewport viewport_{};
    Widget* hovered_{nullptr};
    Widget* focused_{nullptr};
    Widget* captured_{nullptr};
    double mouseX_{0}, mouseY_{0};
    bool cursorInside_{true}, windowFocused_{true};
    bool dirty_{true}, dispatching_{false};
};
} // namespace gui
