// Renderer and Panel tests. Each case renders into an offscreen framebuffer of
// a chosen size, so logical and framebuffer sizes can be set independently,
// then reads pixels back. Requires a display for the hidden GLFW window.

#include "gui/Panel.hpp"
#include "gui/Button.hpp"
#include "gui/UIContext.hpp"
#include "gui/Font.hpp"
#include "gui/Renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace {

// Test-only GL functions, loaded separately from the renderer's own loader.
struct TestGL {
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers{};
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer{};
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers{};
    PFNGLGENRENDERBUFFERSPROC GenRenderbuffers{};
    PFNGLBINDRENDERBUFFERPROC BindRenderbuffer{};
    PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers{};
    PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage{};
    PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer{};
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus{};
    PFNGLREADPIXELSPROC ReadPixels{};
    PFNGLGETERRORPROC GetError{};
};

template <typename Function>
void loadFunction(Function& function, const char* name) {
    function = reinterpret_cast<Function>(glfwGetProcAddress(name));

    if (!function) {
        throw std::runtime_error(std::string{"Missing OpenGL function: "} + name);
    }
}

TestGL loadTestGL() {
    TestGL gl;
    loadFunction(gl.GenFramebuffers, "glGenFramebuffers");
    loadFunction(gl.BindFramebuffer, "glBindFramebuffer");
    loadFunction(gl.DeleteFramebuffers, "glDeleteFramebuffers");
    loadFunction(gl.GenRenderbuffers, "glGenRenderbuffers");
    loadFunction(gl.BindRenderbuffer, "glBindRenderbuffer");
    loadFunction(gl.DeleteRenderbuffers, "glDeleteRenderbuffers");
    loadFunction(gl.RenderbufferStorage, "glRenderbufferStorage");
    loadFunction(gl.FramebufferRenderbuffer, "glFramebufferRenderbuffer");
    loadFunction(gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
    loadFunction(gl.ReadPixels, "glReadPixels");
    loadFunction(gl.GetError, "glGetError");
    return gl;
}

// Hidden window that exists only to own an OpenGL 3.3 core context.
class Context {
public:
    Context() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

        window_ = glfwCreateWindow(64, 64, "MyGUI tests", nullptr, nullptr);

        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("Failed to create OpenGL context");
        }

        glfwMakeContextCurrent(window_);
    }

    ~Context() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    GLFWwindow* window_{nullptr};
};

// Offscreen RGBA8 framebuffer, bound for the lifetime of the object.
class Target {
public:
    Target(const TestGL& gl, int width, int height)
        : gl_{gl}, width_{width}, height_{height} {
        gl_.GenRenderbuffers(1, &colorBuffer_);
        gl_.BindRenderbuffer(GL_RENDERBUFFER, colorBuffer_);
        gl_.RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);

        gl_.GenFramebuffers(1, &framebuffer_);
        gl_.BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer_);

        if (gl_.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Offscreen framebuffer is incomplete");
        }
    }

    ~Target() {
        gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl_.DeleteFramebuffers(1, &framebuffer_);
        gl_.DeleteRenderbuffers(1, &colorBuffer_);
    }

    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;

    std::vector<unsigned char> pixels() const {
        std::vector<unsigned char> rgba(static_cast<std::size_t>(width_) * height_ * 4);
        gl_.ReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        return rgba;
    }

    // Reads the pixel at physical coordinates with a top-left origin.
    gui::Color pixel(int x, int y) const {
        unsigned char rgba[4]{};
        gl_.ReadPixels(x, height_ - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

        return gui::Color{rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f};
    }

private:
    const TestGL& gl_;
    int width_{0}, height_{0};
    GLuint framebuffer_{0};
    GLuint colorBuffer_{0};
};

int failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());

    if (!ok) {
        ++failures;
    }
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.02f;
}

bool sameRgb(const gui::Color& a, const gui::Color& b) {
    return near(a.r, b.r) && near(a.g, b.g) && near(a.b, b.b);
}

void expectPixel(const Target& target, int x, int y, const gui::Color& want, const std::string& what) {
    const gui::Color got = target.pixel(x, y);
    char detail[128];
    std::snprintf(detail, sizeof(detail), " at (%d, %d): got %.3f %.3f %.3f, want %.3f %.3f %.3f",
                  x, y, static_cast<double>(got.r), static_cast<double>(got.g), static_cast<double>(got.b),
                  static_cast<double>(want.r), static_cast<double>(want.g), static_cast<double>(want.b));
    check(sameRgb(got, want), what + detail);
}

void expectNoGLError(const TestGL& gl, const std::string& what) {
    const GLenum error = gl.GetError();
    check(error == GL_NO_ERROR, what + ": no OpenGL error");
}

constexpr gui::Color background{0.2f, 0.4f, 0.6f, 1.0f};
constexpr gui::Color red{1.0f, 0.0f, 0.0f, 1.0f};
constexpr gui::Color green{0.0f, 1.0f, 0.0f, 1.0f};

gui::Viewport makeViewport(int logicalWidth, int logicalHeight, int framebufferWidth, int framebufferHeight) {
    return gui::Viewport{
        .logicalWidth = logicalWidth,
        .logicalHeight = logicalHeight,
        .framebufferWidth = framebufferWidth,
        .framebufferHeight = framebufferHeight
    };
}

void renderPanels(gui::Renderer& renderer, const gui::Viewport& viewport,
                  std::initializer_list<const gui::Panel*> panels) {
    renderer.beginFrame(viewport);
    renderer.clear(background);

    for (const gui::Panel* panel : panels) {
        panel->draw(renderer);
    }

    renderer.endFrame();
}

// Checks that the logical rect covers exactly the given physical pixel span,
// sampling just inside and just outside every edge.
void expectCovers(const Target& target, int left, int top, int right, int bottom,
                  const gui::Color& color, const std::string& what) {
    expectPixel(target, left, top, color, what + " top-left inside");
    expectPixel(target, right - 1, bottom - 1, color, what + " bottom-right inside");
    expectPixel(target, left - 1, top, background, what + " left edge outside");
    expectPixel(target, right, top, background, what + " right edge outside");
    expectPixel(target, left, top - 1, background, what + " top edge outside");
    expectPixel(target, left, bottom, background, what + " bottom edge outside");
}

void testClear(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 800, 600};
    renderer.beginFrame(makeViewport(800, 600, 800, 600));
    renderer.clear(background);
    renderer.endFrame();

    expectPixel(target, 0, 0, background, "clear: top-left corner");
    expectPixel(target, 799, 599, background, "clear: bottom-right corner");
    expectPixel(target, 400, 300, background, "clear: center");
    expectNoGLError(gl, "clear");
}

void testClearDiscardsQueuedRects(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 800, 600};
    renderer.beginFrame(makeViewport(800, 600, 800, 600));
    renderer.drawRect({0.0f, 0.0f, 100.0f, 100.0f}, red);
    renderer.clear(background);
    renderer.endFrame();

    expectPixel(target, 50, 50, background, "clear discards rects queued before it");
    expectNoGLError(gl, "clear discards queued rects");
}

void testPanelPlacement(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 800, 600};
    gui::Panel panel{{10.0f, 20.0f, 100.0f, 50.0f}};
    panel.setBackgroundColor(red);

    renderPanels(renderer, makeViewport(800, 600, 800, 600), {&panel});
    expectCovers(target, 10, 20, 110, 70, red, "panel placement:");
    expectNoGLError(gl, "panel placement");
}

void testVisibility(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 800, 600};
    gui::Panel panel{{10.0f, 20.0f, 100.0f, 50.0f}};
    panel.setBackgroundColor(red);
    panel.setVisible(false);

    renderPanels(renderer, makeViewport(800, 600, 800, 600), {&panel});
    expectPixel(target, 10, 20, background, "hidden panel: top-left");
    expectPixel(target, 60, 45, background, "hidden panel: center");
    expectPixel(target, 109, 69, background, "hidden panel: bottom-right");

    panel.setVisible(true);
    renderPanels(renderer, makeViewport(800, 600, 800, 600), {&panel});
    expectPixel(target, 60, 45, red, "panel visible again");
    expectNoGLError(gl, "visibility");
}

void testBackgroundColor(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 800, 600};
    gui::Panel panel{{10.0f, 20.0f, 100.0f, 50.0f}};
    panel.setBackgroundColor(red);

    renderPanels(renderer, makeViewport(800, 600, 800, 600), {&panel});
    expectPixel(target, 60, 45, red, "background color: red");

    panel.setBackgroundColor(green);
    check(sameRgb(panel.backgroundColor(), green), "background color: getter returns new color");
    renderPanels(renderer, makeViewport(800, 600, 800, 600), {&panel});
    expectPixel(target, 60, 45, green, "background color: green after change");
    expectNoGLError(gl, "background color");
}

void testHiDpi(gui::Renderer& renderer, const TestGL& gl) {
    const Target target{gl, 1200, 900};
    const gui::Viewport viewport = makeViewport(800, 600, 1200, 900);
    check(near(viewport.scaleX(), 1.5f) && near(viewport.scaleY(), 1.5f), "HiDPI: scale is 1.5");

    gui::Panel panel{{20.0f, 20.0f, 200.0f, 100.0f}};
    panel.setBackgroundColor(red);

    renderPanels(renderer, viewport, {&panel});
    expectCovers(target, 30, 30, 330, 180, red, "HiDPI 800x600 -> 1200x900:");
    expectNoGLError(gl, "HiDPI");
}

void testResize(gui::Renderer& renderer, const TestGL& gl) {
    struct Case {
        int logicalWidth;
        int logicalHeight;
        int framebufferWidth;
        int framebufferHeight;
    };

    for (const Case& size : {Case{800, 600, 1200, 900}, Case{1000, 700, 1500, 1050}, Case{1000, 700, 1000, 700}}) {
        const Target target{gl, size.framebufferWidth, size.framebufferHeight};
        const gui::Viewport viewport = makeViewport(
            size.logicalWidth, size.logicalHeight, size.framebufferWidth, size.framebufferHeight);
        const float scale = viewport.scaleX();

        gui::Panel topLeft{{20.0f, 20.0f, 200.0f, 100.0f}};
        topLeft.setBackgroundColor(red);

        const float w = static_cast<float>(size.logicalWidth);
        const float h = static_cast<float>(size.logicalHeight);
        gui::Panel bottomRight{{w - 100.0f, h - 50.0f, 100.0f, 50.0f}};
        bottomRight.setBackgroundColor(green);

        renderPanels(renderer, viewport, {&topLeft, &bottomRight});

        const std::string label = "resize " + std::to_string(size.logicalWidth) + "x"
            + std::to_string(size.logicalHeight) + " -> " + std::to_string(size.framebufferWidth) + "x"
            + std::to_string(size.framebufferHeight) + ":";
        const auto physical = [scale](float logical) { return static_cast<int>(std::lround(logical * scale)); };

        expectCovers(target, physical(20.0f), physical(20.0f), physical(220.0f), physical(120.0f), red,
                     label + " top-left panel");
        expectPixel(target, size.framebufferWidth - 1, size.framebufferHeight - 1, green,
                    label + " bottom-right panel in corner");
        expectPixel(target, physical(w - 100.0f) - 1, size.framebufferHeight - 1, background,
                    label + " left of bottom-right panel");
        expectNoGLError(gl, label);
    }
}

void testNestedClips(gui::Renderer& renderer, const TestGL& gl) {
    for (const auto size : {gui::Size{200, 120}, gui::Size{300, 180}, gui::Size{400, 240}, gui::Size{400, 180}}) {
        const auto viewport = makeViewport(200, 120, static_cast<int>(size.width), static_cast<int>(size.height));
        const Target target{gl, viewport.framebufferWidth, viewport.framebufferHeight};
        renderer.beginFrame(viewport); renderer.clear(background);
        renderer.pushClip({10, 10, 100, 80}); renderer.drawRect({0, 0, 200, 120}, red);
        renderer.pushClip({60, 40, 100, 70}); renderer.drawRect({0, 0, 200, 120}, green);
        renderer.popClip(); renderer.popClip();
        renderer.drawRect({150, 10, 20, 20}, red);
        renderer.pushClip({1, 1, 0, 10}); renderer.drawRect({0, 0, 200, 120}, green); renderer.popClip();
        renderer.endFrame();
        const auto sample = [&](int x, int y, const gui::Color& color, const char* label) {
            expectPixel(target, static_cast<int>(x * viewport.scaleX()), static_cast<int>(y * viewport.scaleY()), color, label);
        };
        sample(20, 20, red, "parent clip preserves its earlier batch");
        sample(70, 50, green, "nested clip intersects its parent");
        sample(110, 50, background, "nested clip cannot extend beyond parent right edge");
        sample(70, 90, background, "top-left to bottom-left scissor conversion clips bottom edge");
        sample(160, 20, red, "popClip restores unclipped subsequent draws");
        expectNoGLError(gl, "nested clips at independent display scales");
    }
    const Target target{gl, 300, 180};
    renderer.beginFrame(makeViewport(200, 120, 300, 180)); renderer.clear(background);
    renderer.pushClip({10.5f, 10.5f, 20, 20}); renderer.drawRect({0, 0, 200, 120}, red); renderer.popClip(); renderer.endFrame();
    expectCovers(target, 15, 15, 46, 46, red, "fractional clip uses outward-rounded physical edges:");
    renderer.beginFrame(makeViewport(200, 120, 300, 180));
    renderer.pushClip({10, 10, 20, 20}); renderer.drawRect({0, 0, 200, 120}, red);
    renderer.clear(green); renderer.drawRect({0, 0, 200, 120}, red); renderer.popClip(); renderer.endFrame();
    expectPixel(target, 0, 0, green, "clear bypasses clip and discards previous batches");
    expectPixel(target, 20, 20, red, "clear retains the logical clip for subsequent drawing");
    bool caught = false;
    try { renderer.popClip(); } catch (const std::logic_error&) { caught = true; }
    check(caught, "clip underflow is rejected");
    renderer.pushClip({0, 0, 10, 10}); caught = false;
    try { renderer.endFrame(); } catch (const std::logic_error&) { caught = true; }
    check(caught, "unbalanced frame clip stack is rejected");
    renderer.beginFrame(makeViewport(200, 120, 300, 180)); renderer.endFrame();
}

void testUnpackState(gui::Renderer& renderer, const TestGL& gl, const std::shared_ptr<gui::Font>& font) {
    PFNGLPIXELSTOREIPROC pixelStore{};
    PFNGLGETINTEGERVPROC getInteger{};
    loadFunction(pixelStore, "glPixelStorei"); loadFunction(getInteger, "glGetIntegerv");
    constexpr GLenum names[]{GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS};
    constexpr GLint unusual[]{8, 17, 1, 2};
    GLint original[4]{};
    for (int i = 0; i < 4; ++i) { getInteger(names[i], &original[i]); pixelStore(names[i], unusual[i]); }
    const Target target{gl, 100, 80};
    renderer.beginFrame(makeViewport(100, 80, 100, 80)); renderer.clear({0, 0, 0, 1});
    renderer.drawText(font, "Glyph", {5, 5}, 23, red);
    bool restored = true;
    for (int i = 0; i < 4; ++i) {
        GLint current{}; getInteger(names[i], &current); restored &= current == unusual[i];
        pixelStore(names[i], original[i]);
    }
    renderer.endFrame();
    bool visible = false;
    const auto pixels = target.pixels();
    for (std::size_t i = 0; i < pixels.size(); i += 4) visible |= pixels[i] > 100;
    check(restored && visible, "glyph upload ignores inherited pixel unpack state and restores it");
    expectNoGLError(gl, "glyph upload with inherited pixel layout");
}

void testText(gui::Renderer& renderer, const TestGL& gl, const std::shared_ptr<gui::Font>& font) {
    constexpr gui::Color black{0, 0, 0, 1}, white{1, 1, 1, 1};
    for (const auto size : {gui::Size{200, 100}, gui::Size{300, 150}, gui::Size{400, 200}, gui::Size{400, 150}}) {
        const auto viewport = makeViewport(200, 100, static_cast<int>(size.width), static_cast<int>(size.height));
        const Target target{gl, viewport.framebufferWidth, viewport.framebufferHeight};
        renderer.beginFrame(viewport); renderer.clear(black);
        renderer.pushClip({10, 10, 80, 60});
        renderer.drawText(font, "Hello, café!\nUnicode", {10, 10}, 20, white);
        renderer.popClip(); renderer.endFrame();
        const auto pixels = target.pixels();
        std::size_t visible = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4) if (pixels[i] > 20) ++visible;
        check(visible > 100, "text produces visible glyph coverage at normal, HiDPI, and nonuniform scales");
        const int right = static_cast<int>(90 * viewport.scaleX());
        for (int y = 0; y < viewport.framebufferHeight; y += 5) expectPixel(target, right, y, black, "text stays inside its clip");
        expectNoGLError(gl, "text at independent display scales");
    }
    const Target target{gl, 200, 100};
    renderer.beginFrame(makeViewport(200, 100, 200, 100)); renderer.clear(black);
    renderer.drawText(font, "Opaque then covered", {5, 5}, 20, white);
    renderer.drawRect({0, 0, 200, 50}, red);
    renderer.drawText(font, "Later text", {5, 50}, 20, green);
    renderer.endFrame();
    expectPixel(target, 15, 20, red, "later rectangle covers earlier text");
    bool greenFound = false;
    const auto ordered = target.pixels();
    for (std::size_t i = 0; i < ordered.size(); i += 4) greenFound |= ordered[i + 1] > 100;
    check(greenFound, "later text renders after rectangles without losing texture binding");
    renderer.beginFrame(makeViewport(200, 100, 200, 100)); renderer.clear(black);
    renderer.drawText(font, "Alpha", {5, 5}, 30, {0.8f, 0, 0, 0.5f}); renderer.endFrame();
    unsigned char maximum = 0;
    const auto translucent = target.pixels();
    for (std::size_t i = 0; i < translucent.size(); i += 4) maximum = std::max(maximum, translucent[i]);
    check(maximum > 60 && maximum <= 104, "glyph coverage multiplies text alpha before blending");
    renderer.beginFrame(makeViewport(200, 100, 200, 100)); renderer.clear(black);
    renderer.drawText(font, "\xF4\x8F\xBF\xBF", {5, 5}, 30, white); renderer.endFrame();
    bool fallbackFound = false;
    const auto fallback = target.pixels();
    for (std::size_t i = 0; i < fallback.size(); i += 4) fallbackFound |= fallback[i] > 100;
    check(fallbackFound, "unsupported Unicode renders a visible fallback glyph");
    expectNoGLError(gl, "text ordering, transparency and fallback");
}

void testAtlasGrowth(gui::Renderer& renderer, const TestGL& gl, const std::shared_ptr<gui::Font>& font) {
    const Target target{gl, 100, 80};
    const auto viewport = makeViewport(100, 80, 100, 80);
    renderer.beginFrame(viewport); renderer.clear({0, 0, 0, 1});
    renderer.drawText(font, "A", {10, 10}, 32, red); renderer.endFrame();
    const auto before = target.pixels();
    renderer.beginFrame(viewport); renderer.clear({0, 0, 0, 1});
    renderer.drawText(font, "A", {10, 10}, 32, red);
    // Allocate several atlas pages while the first glyph is still queued.
    for (unsigned int cp = 0x100; cp < 0x200; ++cp) {
        const std::string text{static_cast<char>(0xC0 | (cp >> 6)), static_cast<char>(0x80 | (cp & 0x3F))};
        renderer.drawText(font, text, {10000, 10000}, 200, green);
    }
    renderer.endFrame();
    check(before == target.pixels(), "atlas growth preserves queued glyph texture coordinates and ownership");
    expectNoGLError(gl, "glyph atlas growth");
}

void testWidgetRendering(gui::Renderer& renderer, const TestGL& gl, const std::shared_ptr<gui::Font>& font) {
    const Target target{gl, 640, 360};
    const auto viewport = makeViewport(640, 360, 640, 360);
    gui::UIContext ui;
    auto& root = ui.root(); root.setLayout(gui::Layout::Vertical); root.setPadding({24, 24, 24, 24}); root.setSpacing(12);
    root.setBackgroundColor({0.08f, 0.10f, 0.14f, 1});
    auto& label = root.emplace<gui::Label>("MyGUI — text and interactive controls", font); label.setFontSize(24);
    auto& row = root.emplace<gui::Container>(); row.setLayout(gui::Layout::Horizontal); row.setSpacing(12); row.setBackgroundColor({0, 0, 0, 0});
    auto& normal = row.emplace<gui::Button>("Click me", font);
    auto& disabled = row.emplace<gui::Button>("Disabled", font); disabled.setEnabled(false);
    ui.layout(viewport); ui.requestFocus(&normal);
    renderer.beginFrame(viewport); renderer.clear(background); ui.draw(renderer); renderer.endFrame();
    expectPixel(target, static_cast<int>(disabled.bounds().x + 3), static_cast<int>(disabled.bounds().y + 3), disabled.style().disabled, "disabled button uses its style");
    expectPixel(target, static_cast<int>(std::ceil(normal.bounds().x)), static_cast<int>(std::ceil(normal.bounds().y)), normal.style().focus, "focused button displays a focus border");
    expectNoGLError(gl, "widget tree rendering");
    if (const auto* filename = std::getenv("MYGUI_RENDER_SNAPSHOT")) {
        std::ofstream stream(filename, std::ios::binary);
        if (!stream) throw std::runtime_error("Cannot write render snapshot");
        stream << "P6\n640 360\n255\n";
        const auto rgba = target.pixels();
        for (int y = 359; y >= 0; --y) for (int x = 0; x < 640; ++x)
            stream.write(reinterpret_cast<const char*>(rgba.data() + (static_cast<std::size_t>(y) * 640 + x) * 4), 3);
    }
}

} // namespace

int main() {
    try {
        const Context context;
        const TestGL gl = loadTestGL();
        gui::Renderer renderer{glfwGetProcAddress};

        testClear(renderer, gl);
        testClearDiscardsQueuedRects(renderer, gl);
        testPanelPlacement(renderer, gl);
        testVisibility(renderer, gl);
        testBackgroundColor(renderer, gl);
        testHiDpi(renderer, gl);
        testResize(renderer, gl);
        testNestedClips(renderer, gl);
        const auto font = gui::Font::load(MYGUI_TEST_FONT);
        testUnpackState(renderer, gl, font);
        testText(renderer, gl, font);
        testAtlasGrowth(renderer, gl, font);
        testWidgetRendering(renderer, gl, font);
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }

    std::printf("%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
