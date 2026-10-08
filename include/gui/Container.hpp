#pragma once

#include "gui/Panel.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace gui {
enum class Layout { Absolute, Horizontal, Vertical };

class Container : public Panel {
public:
    using Panel::Panel;
    Container() = default;
    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;
    Container(Container&&) = delete;
    Container& operator=(Container&&) = delete;

    Widget& add(std::unique_ptr<Widget> child);
    template<class T, class... Args> T& emplace(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        auto& result = *child;
        add(std::move(child));
        return result;
    }
    // During event dispatch, removal is committed after the handler returns.
    bool remove(Widget& child);
    const std::vector<std::unique_ptr<Widget>>& children() const noexcept { return children_; }
    void setLayout(Layout layout) noexcept;
    Layout layout() const noexcept { return layout_; }
    void setPadding(Insets padding) noexcept;
    Insets padding() const noexcept { return padding_; }
    void setSpacing(float spacing) noexcept;
    float spacing() const noexcept { return spacing_; }
    void setClipChildren(bool clip) noexcept { clipChildren_ = clip; invalidateLayout(); }
    bool clipChildren() const noexcept { return clipChildren_; }
    Rect contentBounds() const noexcept;
    Size preferredSize() const noexcept override;
    virtual void arrange();
    void draw(Renderer& renderer) const override;

private:
    friend class UIContext;
    void attach(UIContext* context) noexcept;
    void commitMutations();
    Layout layout_{Layout::Absolute};
    Insets padding_{};
    float spacing_{0};
    bool clipChildren_{true};
    std::vector<std::unique_ptr<Widget>> children_, pendingChildren_;
};
} // namespace gui
