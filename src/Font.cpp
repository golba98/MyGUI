#include "gui/Font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gui {
std::u32string decodeUtf8(std::string_view text) {
    std::u32string result;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) { result.push_back(first); ++i; continue; }
        const int count = first >= 0xC2 && first <= 0xDF ? 2
            : first >= 0xE0 && first <= 0xEF ? 3 : first >= 0xF0 && first <= 0xF4 ? 4 : 0;
        char32_t codepoint = count ? first & ((1u << (7 - count)) - 1) : 0;
        bool valid = count && i + static_cast<std::size_t>(count) <= text.size();
        if (valid) {
            for (int j = 1; j < count; ++j) {
                const auto byte = static_cast<unsigned char>(text[i + static_cast<std::size_t>(j)]);
                if ((byte & 0xC0) != 0x80) { valid = false; break; }
                codepoint = (codepoint << 6) | (byte & 0x3F);
            }
            const char32_t minimum = count == 2 ? 0x80 : count == 3 ? 0x800 : 0x10000;
            valid = valid && codepoint >= minimum && codepoint <= 0x10FFFF
                && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
        }
        result.push_back(valid ? codepoint : U'\uFFFD');
        i += valid ? static_cast<std::size_t>(count) : 1;
    }
    return result;
}
struct Font::Impl {
    FT_Library library{nullptr};
    FT_Face face{nullptr};
    ~Impl() { if (face) FT_Done_Face(face); if (library) FT_Done_FreeType(library); }
    FT_UInt glyph(char32_t codepoint) const {
        auto index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
        if (!index) index = FT_Get_Char_Index(face, 0xFFFD);
        if (!index) index = FT_Get_Char_Index(face, '?');
        return index; // Final fallback: the font's .notdef glyph.
    }
};
Font::Font(const std::string& filename) : impl_{std::make_unique<Impl>()} {
    if (FT_Init_FreeType(&impl_->library)) throw std::runtime_error("Failed to initialize FreeType");
    if (FT_New_Face(impl_->library, filename.c_str(), 0, &impl_->face))
        throw std::runtime_error("Failed to load font: " + filename);
    if (!FT_IS_SCALABLE(impl_->face) || !impl_->face->units_per_EM)
        throw std::runtime_error("Font must be scalable: " + filename);
    if (FT_Select_Charmap(impl_->face, FT_ENCODING_UNICODE))
        throw std::runtime_error("Font has no Unicode character map: " + filename);
}
Font::~Font() = default;
std::shared_ptr<Font> Font::load(const std::string& filename) { return std::shared_ptr<Font>(new Font(filename)); }
TextMetrics Font::shape(std::string_view text, float size, std::vector<PositionedGlyph>* glyphs) const {
    if (!std::isfinite(size) || size <= 0 || size > 4096) throw std::invalid_argument("Font size must be in (0, 4096]");
    const auto face = impl_->face;
    const float scale = size / static_cast<float>(face->units_per_EM);
    TextMetrics metrics;
    metrics.ascent = static_cast<float>(face->ascender) * scale;
    metrics.descent = -static_cast<float>(face->descender) * scale;
    metrics.lineHeight = static_cast<float>(face->height) * scale;
    float x = 0, baseline = metrics.ascent;
    FT_UInt previous = 0;
    const auto append = [&](char32_t codepoint) {
        const auto index = impl_->glyph(codepoint);
        if (previous && index && FT_HAS_KERNING(face)) {
            FT_Vector kerning{};
            if (FT_Get_Kerning(face, previous, index, FT_KERNING_UNSCALED, &kerning))
                throw std::runtime_error("Failed to measure glyph kerning");
            x += static_cast<float>(kerning.x) * scale;
        }
        if (FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING))
            throw std::runtime_error("Failed to measure glyph");
        if (glyphs) glyphs->push_back({codepoint, x, baseline});
        x += static_cast<float>(face->glyph->advance.x) * scale;
        previous = index;
    };
    const auto codepoints = decodeUtf8(text);
    std::size_t lines = codepoints.empty() ? 0 : 1;
    for (const auto codepoint : codepoints) {
        if (codepoint == U'\r') continue;
        if (codepoint == U'\n') {
            metrics.width = std::max(metrics.width, x);
            x = 0; baseline += metrics.lineHeight; previous = 0; ++lines;
        }
        else if (codepoint == U'\t') { for (int i = 0; i < 4; ++i) append(U' '); }
        else append(codepoint);
    }
    metrics.width = std::max(metrics.width, x);
    metrics.height = static_cast<float>(lines) * metrics.lineHeight;
    return metrics;
}
TextMetrics Font::measureText(std::string_view text, float logicalSize) const { return shape(text, logicalSize, nullptr); }
Font::Bitmap Font::rasterize(char32_t codepoint, unsigned int width, unsigned int height) const {
    if (!width || !height || width > 4096 || height > 4096) throw std::invalid_argument("Raster size must be in [1, 4096]");
    const auto face = impl_->face;
    if (FT_Set_Pixel_Sizes(face, width, height)
        || FT_Load_Glyph(face, impl_->glyph(codepoint), FT_LOAD_RENDER | FT_LOAD_NO_HINTING))
        throw std::runtime_error("Failed to rasterize glyph");
    const auto& source = face->glyph->bitmap;
    Bitmap result;
    result.width = static_cast<int>(source.width); result.height = static_cast<int>(source.rows);
    result.left = face->glyph->bitmap_left; result.top = face->glyph->bitmap_top;
    result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);
    for (int y = 0; y < result.height; ++y) {
        const auto* row = source.buffer + y * source.pitch;
        for (int x = 0; x < result.width; ++x) {
            result.pixels[static_cast<std::size_t>(y) * result.width + x] = source.pixel_mode == FT_PIXEL_MODE_MONO
                ? ((row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0) : row[x];
        }
    }
    return result;
}
} // namespace gui
