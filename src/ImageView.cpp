#include "gui/ImageView.hpp"
#include "gui/Renderer.hpp"

#include <algorithm>

namespace gui {
ImageView::ImageView(std::shared_ptr<Image> image, ImageScaling scaling) : image_{std::move(image)}, scaling_{scaling} {}
void ImageView::setImage(std::shared_ptr<Image> image) { image_ = std::move(image); invalidateLayout(); }
Size ImageView::preferredSize() const noexcept {
    const auto preferred = Widget::preferredSize();
    if (!image_) return preferred;
    return {std::max(preferred.width, static_cast<float>(image_->width())),
            std::max(preferred.height, static_cast<float>(image_->height()))};
}
Rect ImageView::imageRect() const noexcept {
    const auto& b = bounds();
    if (!image_ || scaling_ == ImageScaling::Stretch) return b;
    const auto width = static_cast<float>(image_->width()), height = static_cast<float>(image_->height());
    const float scale = scaling_ == ImageScaling::Fit ? std::min(b.width / width, b.height / height) : 1.0f;
    return {b.x + (b.width - width * scale) / 2, b.y + (b.height - height * scale) / 2, width * scale, height * scale};
}
void ImageView::draw(Renderer& renderer) const {
    if (!visible() || !image_ || bounds().empty()) return;
    renderer.pushClip(bounds());
    try { renderer.drawImage(image_, imageRect(), tint_); }
    catch (...) { renderer.popClip(); throw; }
    renderer.popClip();
}
} // namespace gui
