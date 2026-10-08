#pragma once

#include "gui/Geometry.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gui {
struct TextMetrics {
    float width{0}, height{0}, ascent{0}, descent{0}, lineHeight{0};
};

// Replaces invalid UTF-8 bytes with U+FFFD. No platform or display dependency.
std::u32string decodeUtf8(std::string_view text);

// CPU font resources. Renderer owns all associated GPU textures.
class Font {
public:
    static std::shared_ptr<Font> load(const std::string& filename);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    TextMetrics measureText(std::string_view text, float logicalSize) const;

private:
    friend class Renderer;
    explicit Font(const std::string& filename);
    struct PositionedGlyph { char32_t codepoint; float x, baseline; };
    struct Bitmap {
        int width{0}, height{0}, left{0}, top{0};
        std::vector<std::uint8_t> pixels;
    };
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TextMetrics shape(std::string_view text, float size, std::vector<PositionedGlyph>* glyphs) const;
    Bitmap rasterize(char32_t codepoint, unsigned int width, unsigned int height) const;
};
} // namespace gui
