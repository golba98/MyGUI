#pragma once

#include "gui/Geometry.hpp"

namespace gui {
class Renderer;
class Event;
class Container;
class UIContext;

// Bounds remain in logical window coordinates, even inside a container.
class Widget {
public:
    virtual ~Widget() = default;
    virtual void draw(Renderer& renderer) const = 0;
    virtual bool onEvent(const Event&, UIContext&) { return false; }
    virtual void cancelInteraction() noexcept {}
    virtual Size preferredSize() const noexcept { return preferredSize_; }

    void setBounds(const Rect& bounds) noexcept;
    const Rect& bounds() const noexcept { return bounds_; }
    bool hitTest(double x, double y) const noexcept { return bounds_.contains(x, y); }
    void setVisible(bool visible) noexcept;
    bool visible() const noexcept { return visible_; }
    void setEnabled(bool enabled) noexcept;
    bool enabled() const noexcept { return enabled_; }
    void setFocusable(bool focusable) noexcept;
    bool focusable() const noexcept { return focusable_; }
    bool hovered() const noexcept { return hovered_; }
    bool focused() const noexcept { return focused_; }
    Container* parent() const noexcept { return parent_; }
    void setMinimumSize(Size size) noexcept;
    Size minimumSize() const noexcept { return minimumSize_; }
    void setPreferredSize(Size size) noexcept;
    void setFlex(float weight) noexcept;
    float flex() const noexcept { return flex_; }

protected:
    Widget() = default;
    explicit Widget(const Rect& bounds) noexcept;
    // Copies retain visual properties, never attachment or interaction state.
    Widget(const Widget& other) noexcept;
    Widget& operator=(const Widget& other) noexcept;
    Widget(Widget&& other) noexcept : Widget(static_cast<const Widget&>(other)) {}
    Widget& operator=(Widget&& other) noexcept { return operator=(static_cast<const Widget&>(other)); }
    void invalidateLayout() noexcept;

private:
    friend class Container;
    friend class UIContext;
    Rect bounds_{};
    Size minimumSize_{}, preferredSize_{};
    float flex_{0};
    bool visible_{true}, enabled_{true}, focusable_{false};
    bool hovered_{false}, focused_{false}, pendingRemoval_{false};
    Container* parent_{nullptr};
    UIContext* context_{nullptr};
};
} // namespace gui
