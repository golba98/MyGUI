#include "gui/Font.hpp"
#include "gui/Label.hpp"
#include "gui/ScrollView.hpp"
#include "gui/UIContext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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
        std::string encoded;
        for (const char32_t cp : {U'A', U'\u00E9', U'\u20AC', U'\U0001F642', char32_t{0xD800}, char32_t{0x110000}}) gui::appendUtf8(encoded, cp);
        check(gui::decodeUtf8(encoded) == U"A\u00E9\u20AC\U0001F642\uFFFD\uFFFD", "UTF-8 encoding round-trips and replaces invalid code points");
        check(gui::sanitizeUtf8("a\xFF" "b") == "a\xEF\xBF\xBD" "b", "sanitizing replaces invalid bytes");
        const std::string mixed = "a\xC3\xA9\xF0\x9F\x99\x82";
        check(gui::nextUtf8Boundary(mixed, 0) == 1 && gui::nextUtf8Boundary(mixed, 1) == 3 && gui::nextUtf8Boundary(mixed, 3) == 7
              && gui::nextUtf8Boundary(mixed, 7) == 7, "next boundaries skip continuation bytes");
        check(gui::previousUtf8Boundary(mixed, 7) == 3 && gui::previousUtf8Boundary(mixed, 3) == 1
              && gui::previousUtf8Boundary(mixed, 0) == 0 && gui::previousUtf8Boundary(mixed, 99) == 3, "previous boundaries clamp and skip continuations");
        const auto font = gui::Font::load(MYGUI_TEST_FONT);
        const auto offsets = font->caretOffsets("A\xC3\xA9" "B", 16);
        check(offsets.size() == 4 && offsets[0] == 0 && offsets[1] < offsets[2] && offsets[2] < offsets[3]
              && near(offsets[3], font->measureText("A\xC3\xA9" "B", 16).width), "caret offsets mark each code point boundary");
        check(font->caretOffsets("", 16) == std::vector<float>{0}, "empty text has one caret position");
        const auto width = [&](const char* text) { return font->measureText(text, 16).width; };
        const float twoWords = (width("hello") + width("hello world")) / 2;
        check(font->wrapText("hello world", 16, twoWords) == "hello\nworld", "wrapping breaks at the space that overflows");
        check(font->wrapText("hello world", 16, width("hello world") + 1) == "hello world", "text that fits is unchanged");
        check(font->wrapText("one two three four", 16, width("one two three") - 1) == "one two\nthree four",
              "wrapping fills each line greedily");
        check(font->wrapText("hello   world", 16, twoWords) == "hello\nworld", "consecutive spaces at a break are consumed");
        check(font->wrapText("a\nhello world", 16, twoWords) == "a\nhello\nworld", "explicit newlines are kept");
        const auto split = font->wrapText("abcdefghijklmnop", 16, width("abcde"));
        bool narrow = split.find('\n') != std::string::npos;
        for (std::size_t start = 0; start <= split.size();) {
            const auto end = std::min(split.find('\n', start), split.size());
            narrow &= width(split.substr(start, end - start).c_str()) <= width("abcde") + 0.01f;
            start = end + 1;
        }
        check(narrow, "long words are split so every line fits");
        check(font->wrapText("\xC3\xA9\xC3\xA9\xC3\xA9", 16, 1) == "\xC3\xA9\n\xC3\xA9\n\xC3\xA9", "narrow wrapping keeps code points intact and makes progress");
        check(font->wrapText("hello world", 16, 0) == "hello world", "nonpositive widths disable wrapping");
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

        // Wrapped labels: a vertical layout asks for the height at the final width.
        const std::string sentence = "The quick brown fox jumps over the lazy dog";
        const float line = font->measureText("", 16).lineHeight;
        gui::UIContext wrapUi; auto& column = wrapUi.root();
        column.setLayout(gui::Layout::Vertical); column.setPadding({10, 10, 10, 10});
        auto& paragraph = column.emplace<gui::Label>(sentence, font); paragraph.setWrap(true);
        auto& after = column.emplace<gui::Label>("after", font);
        const auto linesAt = [&](float width) {
            const auto text = font->wrapText(sentence, 16, width);
            return static_cast<float>(std::count(text.begin(), text.end(), '\n') + 1);
        };
        wrapUi.layout({400, 300, 400, 300});
        check(near(paragraph.bounds().height, line * linesAt(380)) && linesAt(380) == 1, "a wide wrapped label keeps one line");
        wrapUi.layout({150, 300, 150, 300});
        check(linesAt(130) > 2 && near(paragraph.bounds().height, line * linesAt(130)), "narrowing the layout makes a wrapped label taller");
        check(near(after.bounds().y, paragraph.bounds().y + paragraph.bounds().height), "following widgets move below the wrapped text");
        check(near(column.heightForWidth(150), line * (linesAt(130) + 1) + 20), "containers report the wrapped height for a width");
        paragraph.setWrap(false); wrapUi.layout({150, 300, 150, 300});
        check(near(paragraph.bounds().height, line), "unwrapped labels keep their natural height");

        gui::UIContext scrollUi;
        auto& view = scrollUi.root().emplace<gui::ScrollView>(); view.setBounds({0, 0, 120, 40});
        auto& long_ = view.content().emplace<gui::Label>(sentence, font); long_.setWrap(true);
        scrollUi.layout({120, 40, 120, 40});
        const float available = 120 - gui::ScrollView::scrollbarWidth;
        check(near(view.contentSize().height, line * linesAt(available)) && view.maxScrollOffset().y > 0,
              "scroll views size wrapped content for the width beside the scrollbar");
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Error: %s\n", error.what()); return 1; }
    return failures ? 1 : 0;
}
