#include "gui/Font.hpp"
#include "gui/Label.hpp"
#include "gui/UIContext.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }
bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }
}
int main() {
    try {
        check(gui::decodeUtf8("A\xC3\xA9\xF0\x9F\x99\x82") == U"Aé🙂", "UTF-8 decodes Latin and supplementary code points");
        check(gui::decodeUtf8("\xFF\xC0\x80") == U"\uFFFD\uFFFD\uFFFD", "invalid and overlong bytes are replaced consistently");
        check(gui::decodeUtf8("\xED\xA0\x80\xF4\x90\x80\x80").find(U'\uFFFD') != std::u32string::npos, "surrogates and out-of-range Unicode are rejected");
        const auto font = gui::Font::load(MYGUI_TEST_FONT);
        const auto a = font->measureText("Hello", 16), b = font->measureText("Hello", 32);
        check(a.width > 0 && a.height > 0 && a.ascent > 0 && a.descent >= 0, "font measurement supplies usable line metrics");
        check(near(b.width, a.width * 2) && near(b.height, a.height * 2), "measurement scales in logical units");
        check(font->measureText("", 16).width == 0 && font->measureText("", 16).height == 0, "empty text has empty dimensions");
        const auto multiline = font->measureText("Hello\nHi", 16);
        check(near(multiline.width, a.width) && near(multiline.height, a.height * 2), "newlines measure maximum width and accumulated height");
        check(near(font->measureText("A\tB", 16).width, font->measureText("A    B", 16).width), "tabs consistently expand to four spaces");
        check(near(font->measureText("\xFF", 16).width, font->measureText("\xEF\xBF\xBD", 16).width), "malformed text uses the same replacement glyph as valid replacement text");
        check(font->measureText("\xF4\x8F\xBF\xBF", 16).width > 0, "missing glyph uses a measurable fallback");
        bool failed = false;
        try { gui::Font::load("/nonexistent/mygui-font.ttf"); } catch (const std::runtime_error&) { failed = true; }
        check(failed, "missing font files report a clear exception");
        failed = false;
        try { font->measureText("test", std::numeric_limits<float>::quiet_NaN()); } catch (const std::invalid_argument&) { failed = true; }
        check(failed, "nonfinite font size is rejected");
        gui::UIContext ui; ui.root().setLayout(gui::Layout::Vertical);
        auto& label = ui.root().emplace<gui::Label>("Hello", font); ui.layout({400, 200, 400, 200});
        const float height = label.bounds().height;
        label.setText("Hello\nHello"); ui.layout({400, 200, 400, 200});
        check(near(label.bounds().height, height * 2), "text changes invalidate attached widget layout");
        label.setFontSize(32); ui.layout({400, 200, 800, 400});
        check(near(label.bounds().height, height * 4), "font-size changes affect layout independently of framebuffer scale");
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Error: %s\n", error.what()); return 1; }
    return failures ? 1 : 0;
}
