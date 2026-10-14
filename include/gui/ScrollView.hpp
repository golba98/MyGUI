#pragma once

#include "gui/Container.hpp"

namespace gui {
// Scrolls one content container inside its padded bounds. Add children to
// content(), not to the view itself. The content gets at least the viewport
// size and grows to its preferred size; vertical scrolling is always available,
// horizontal scrolling only when enabled. Scrollbars occupy a gutter that
// children never cover. The wheel scrolls the innermost view that can still
// move, then bubbles to outer views. PageUp/PageDown bubbling from a focused
// descendant scroll by a page.
class ScrollView : public Container {
public:
    static constexpr float scrollbarWidth = 10;
    static constexpr float lineStep = 40;

    ScrollView();
    Container& content() noexcept { return *content_; }
    const Container& content() const noexcept { return *content_; }
    void setHorizontalScrolling(bool enabled) noexcept { horizontal_ = enabled; invalidateLayout(); }
    bool horizontalScrolling() const noexcept { return horizontal_; }

    // Offsets are clamped to [0, maxScrollOffset()] using the latest layout.
    Point scrollOffset() const noexcept { return offset_; }
    void setScrollOffset(Point offset) noexcept;
    Point maxScrollOffset() const noexcept;
    Size contentSize() const noexcept { return contentSize_; }
    // Visible part of the content in window coordinates, excluding scrollbars.
    Rect viewport() const noexcept { return viewport_; }
    bool dragging() const noexcept { return drag_ != Axis::None; }

    void scrollIntoView(const Rect& rect) noexcept override;
    Rect childClip() const noexcept override { return viewport_; }
    void arrange() override;
    bool onEvent(const Event& event, UIContext& context) override;
    void cancelInteraction() noexcept override { drag_ = Axis::None; }
    void draw(Renderer& renderer) const override;

private:
    enum class Axis { None, Vertical, Horizontal };
    struct Bar { Rect track, thumb; bool visible{false}; };
    Bar bar(Axis axis) const noexcept;
    Container* content_{};
    Point offset_{};
    Size contentSize_{};
    Rect viewport_{};
    bool horizontal_{false};
    Axis drag_{Axis::None};
    float grab_{0};
};
} // namespace gui
