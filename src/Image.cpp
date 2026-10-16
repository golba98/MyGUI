#include "gui/Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 16384
#include <stb_image.h>

#include <climits>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace gui {
Image::Image(int width, int height, std::vector<std::uint8_t> rgba)
    : width_{width}, height_{height}, pixels_{std::move(rgba)} {}

std::shared_ptr<Image> Image::fromPixels(int width, int height, std::vector<std::uint8_t> rgba) {
    if (width <= 0 || height <= 0 || width > maxDimension || height > maxDimension)
        throw std::invalid_argument("Image dimensions must be in [1, 16384]");
    if (rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4)
        throw std::invalid_argument("Image pixels must contain width * height RGBA values");
    return std::shared_ptr<Image>(new Image(width, height, std::move(rgba)));
}

std::shared_ptr<Image> Image::decode(std::span<const std::uint8_t> encoded) {
    if (encoded.empty() || encoded.size() > static_cast<std::size_t>(INT_MAX))
        throw std::runtime_error("Image data is empty or too large");
    int width = 0, height = 0, channels = 0;
    // stb_image reports its failure reason through a global, so only the result is used.
    std::unique_ptr<stbi_uc, void (*)(void*)> pixels{
        stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &channels, 4),
        &stbi_image_free};
    if (!pixels) throw std::runtime_error("Failed to decode image");
    const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    return fromPixels(width, height, std::vector<std::uint8_t>(pixels.get(), pixels.get() + size));
}

std::shared_ptr<Image> Image::load(const std::string& filename) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) throw std::runtime_error("Failed to open image: " + filename);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    try { return decode(bytes); }
    catch (const std::runtime_error&) { throw std::runtime_error("Failed to load image: " + filename); }
}
} // namespace gui
