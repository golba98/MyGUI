#include "gui/Popup.hpp"
#include "gui/Event.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"
namespace gui {
Popup::Popup() { setBackgroundColor({0,0,0,0}); setPadding({4,4,4,4}); }
Size Popup::preferredSize() const noexcept {
    const auto wanted=content().preferredSize(), specified=Widget::preferredSize();
    return {std::max(specified.width,wanted.width+8),std::max(specified.height,wanted.height+8)};
}
void Popup::draw(Renderer& renderer) const {
    if (!visible()) return;
    renderer.drawRoundedRect(bounds(),theme().cornerRadius,theme().surface);
    ScrollView::draw(renderer);
    renderer.drawBorder(bounds(),1,theme().border,theme().cornerRadius);
}
bool Popup::onEvent(const Event& event, UIContext& context) {
    const bool handled=ScrollView::onEvent(event,context);
    return handled || event.getIf<MouseScrollEvent>();
}
}
