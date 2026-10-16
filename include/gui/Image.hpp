#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace gui {
// Immutable CPU pixels: straight-alpha RGBA8, rows from top to bottom.
// Renderers own the GPU textures created from an image.
class Image {
public:
    static constexpr int maxDimension = 16384;

    // Decodes a PNG or JPEG file. Throws std::runtime_error on failure.
    static std::shared_ptr<Image> load(const std::string& filename);
    // Decodes PNG or JPEG bytes, e.g. a resource embedded in the executable.
    static std::shared_ptr<Image> decode(std::span<const std::uint8_t> encoded);
    // Requires width * height * 4 bytes and dimensions in [1, maxDimension].
    static std::shared_ptr<Image> fromPixels(int width, int height, std::vector<std::uint8_t> rgba);

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }

private:
    Image(int width, int height, std::vector<std::uint8_t> rgba);
    int width_{0}, height_{0};
    std::vector<std::uint8_t> pixels_;
};
} // namespace gui
