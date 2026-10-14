#pragma once

#include "gui/Color.hpp"
#include "gui/Image.hpp"
#include "gui/Widget.hpp"
#include <memory>

namespace gui {
enum class ImageScaling {
    Stretch, // Fill the bounds, ignoring the aspect ratio.
    Fit,     // Largest centered size that keeps the aspect ratio.
    Center   // Natural size, centered and clipped to the bounds.
};

class ImageView : public Widget {
public:
    ImageView() = default;
    explicit ImageView(std::shared_ptr<Image> image, ImageScaling scaling = ImageScaling::Fit);
    void setImage(std::shared_ptr<Image> image);
    const std::shared_ptr<Image>& image() const noexcept { return image_; }
    void setScaling(ImageScaling scaling) noexcept { scaling_ = scaling; }
    ImageScaling scaling() const noexcept { return scaling_; }
    void setTint(Color tint) noexcept { tint_ = tint; }
    Color tint() const noexcept { return tint_; }
    // Logical rect the image occupies inside the current bounds.
    Rect imageRect() const noexcept;
    Size preferredSize() const noexcept override;
    void draw(Renderer& renderer) const override;

private:
    std::shared_ptr<Image> image_;
    ImageScaling scaling_{ImageScaling::Fit};
    Color tint_{1, 1, 1, 1};
};
} // namespace gui
