// Image decoding and ImageView geometry. No window, OpenGL context or display.
#include "gui/Image.hpp"
#include "gui/ImageView.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
int failures = 0;
void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++failures; }
bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

// 2x2 RGBA PNG: red, green / blue, half-transparent white.
constexpr std::uint8_t png[]{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D,
    0x24, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x0C, 0x19, 0x18, 0xFE, 0x83, 0x40, 0x03, 0x00, 0x49, 0x49, 0x09, 0x78, 0x9C, 0x3D, 0x3C,
    0xBE, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

template <class Exception, class Function> bool throws(Function function) {
    try { function(); } catch (const Exception&) { return true; }
    return false;
}

void testDecode() {
    const auto image = gui::Image::decode(png);
    const std::vector<std::uint8_t> expected{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};
    check(image->width() == 2 && image->height() == 2, "PNG dimensions are decoded");
    check(image->pixels() == expected, "PNG pixels are straight-alpha RGBA with the top row first");
    const std::uint8_t garbage[]{1, 2, 3, 4, 5};
    check(throws<std::runtime_error>([&] { gui::Image::decode(garbage); }), "undecodable bytes are rejected");
    check(throws<std::runtime_error>([] { gui::Image::decode({}); }), "empty input is rejected");
    check(throws<std::runtime_error>([] { gui::Image::load("/nonexistent/mygui.png"); }), "missing files are rejected");
}

void testFromPixels() {
    const auto image = gui::Image::fromPixels(1, 2, std::vector<std::uint8_t>(8, 7));
    check(image->width() == 1 && image->height() == 2 && image->pixels().size() == 8, "raw pixels are retained");
    check(throws<std::invalid_argument>([] { gui::Image::fromPixels(2, 2, std::vector<std::uint8_t>(15)); }),
          "pixel count must match the dimensions");
    check(throws<std::invalid_argument>([] { gui::Image::fromPixels(0, 2, {}); }), "zero dimensions are rejected");
    check(throws<std::invalid_argument>([] { gui::Image::fromPixels(gui::Image::maxDimension + 1, 1, {}); }),
          "oversized dimensions are rejected");
}

void testImageView() {
    const auto image = gui::Image::fromPixels(40, 20, std::vector<std::uint8_t>(40 * 20 * 4, 255));
    gui::ImageView view{image};
    view.setBounds({10, 10, 100, 100});
    const auto fit = view.imageRect();
    check(near(fit.x, 10) && near(fit.y, 35) && near(fit.width, 100) && near(fit.height, 50),
          "Fit keeps the aspect ratio and centers the image");
    view.setScaling(gui::ImageScaling::Center);
    const auto centered = view.imageRect();
    check(near(centered.x, 40) && near(centered.y, 50) && near(centered.width, 40), "Center keeps the natural size");
    view.setScaling(gui::ImageScaling::Stretch);
    check(view.imageRect() == view.bounds(), "Stretch fills the bounds");
    check(near(view.preferredSize().width, 40) && near(view.preferredSize().height, 20),
          "preferred size is the image size");
    view.setImage(nullptr);
    check(near(view.preferredSize().width, 0), "an empty view prefers no size");
}
} // namespace

int main() {
    try { testDecode(); testFromPixels(); testImageView(); }
    catch (const std::exception& error) { std::fprintf(stderr, "Error: %s\n", error.what()); return 1; }
    std::printf("%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
