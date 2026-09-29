#pragma once

#include "gui/Geometry.hpp"

namespace gui {

class Renderer;

// Base class for everything drawn in the GUI. Bounds are in logical units.
class Widget {
public:
    virtual ~Widget() = default;

    virtual void draw(Renderer& renderer) const = 0;

    void setBounds(const Rect& bounds) noexcept;
    const Rect& bounds() const noexcept;

    void setVisible(bool visible) noexcept;
    bool visible() const noexcept;

protected:
    // Only derived widgets are constructed, copied or moved, which prevents
    // slicing through a Widget reference.
    Widget() = default;
    explicit Widget(const Rect& bounds) noexcept;

    Widget(const Widget&) = default;
    Widget& operator=(const Widget&) = default;
    Widget(Widget&&) = default;
    Widget& operator=(Widget&&) = default;

private:
    Rect bounds_{};
    bool visible_{true};
};

} // namespace gui
