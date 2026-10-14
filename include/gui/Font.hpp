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
// Appends one code point; surrogates and values above U+10FFFF become U+FFFD.
void appendUtf8(std::string& out, char32_t codepoint);
// Re-encodes text so that every invalid byte becomes U+FFFD.
std::string sanitizeUtf8(std::string_view text);
// Adjacent code point boundaries in valid UTF-8, clamped to [0, text.size()].
std::size_t nextUtf8Boundary(std::string_view text, std::size_t offset) noexcept;
std::size_t previousUtf8Boundary(std::string_view text, std::size_t offset) noexcept;

// CPU font resources. Renderer owns all associated GPU textures.
class Font {
public:
    static std::shared_ptr<Font> load(const std::string& filename);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    TextMetrics measureText(std::string_view text, float logicalSize) const;
    // X offset of every code point boundary on a single line: one entry before
    // each decoded code point plus one at the end. Newlines restart at zero.
    std::vector<float> caretOffsets(std::string_view text, float logicalSize) const;
    // Inserts line breaks so no line is wider than maxWidth where possible.
    // Lines break at spaces or tabs, which are consumed; words longer than a
    // line are split between code points. Explicit newlines are kept and
    // invalid UTF-8 becomes U+FFFD. A nonpositive or nonfinite maxWidth
    // returns the text unchanged.
    std::string wrapText(std::string_view text, float logicalSize, float maxWidth) const;

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
    TextMetrics shape(std::string_view text, float size, std::vector<PositionedGlyph>* glyphs,
                      std::vector<float>* boundaries = nullptr) const;
    Bitmap rasterize(char32_t codepoint, unsigned int width, unsigned int height) const;
};
} // namespace gui
