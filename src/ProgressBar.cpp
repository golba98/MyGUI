#include "gui/ProgressBar.hpp"
#include "gui/Renderer.hpp"
#include "gui/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace gui {
void ProgressBar::setValue(float value) noexcept {
    value_ = std::isnan(value) ? 0 : std::clamp(value, 0.0f, 1.0f);
    requestRedraw();
}
Size ProgressBar::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    return {std::max(preferred.width, 160.0f), std::max(preferred.height, 8.0f)};
}
void ProgressBar::draw(Renderer& renderer) const {
    if (!visible() || bounds().empty()) return;
    const auto& t = theme();
    const auto& b = bounds();
    const float radius = b.height / 2;
    renderer.drawRoundedRect(b, radius, t.track);
    if (value_ <= 0) return;
    // Clip a full-width pill so a short fill keeps the rounded left end.
    renderer.pushClip({b.x, b.y, b.width * value_, b.height});
    try { renderer.drawRoundedRect(b, radius, enabled() ? t.fill : t.disabled); }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
} // namespace gui
