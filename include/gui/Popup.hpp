#pragma once
#include "gui/ScrollView.hpp"
namespace gui {
struct PopupOptions { float maxHeight{240}; float gap{2}; };
// Owned by UIContext; populate content() before opening.
class Popup : public ScrollView {
public:
    Popup();
    Size preferredSize() const noexcept override;
    void draw(Renderer&) const override;
    bool onEvent(const Event&, UIContext&) override;
};
}
