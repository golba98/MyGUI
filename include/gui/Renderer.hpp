#pragma once

#include "gui/Color.hpp"
#include "gui/Geometry.hpp"
#include "gui/Viewport.hpp"
#include "gui/detail/GLHandle.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace gui {

class Font;
class Image;

// Returns the address of an OpenGL function, or nullptr if it is unavailable.
// glfwGetProcAddress has exactly this signature.
using GLProc = void (*)();
using GLProcLoader = GLProc (*)(const char* name);

// Batched 2D renderer for rectangles, text and images using OpenGL 3.3 core.
//
// A Renderer must be created and destroyed while its OpenGL context is
// current, so declare it after the Window that owns the context.
//
// Draw calls are queued between beginFrame() and endFrame() and submitted in
// order, so later rectangles are blended over earlier ones. Coordinates are
// logical units; the renderer maps them onto the framebuffer's pixels.
class Renderer {
public:
    explicit Renderer(GLProcLoader loader);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    // Starts a frame. The logical size defines the coordinate space for draw
    // calls; the framebuffer size sets the OpenGL viewport.
    void beginFrame(const Viewport& viewport);

    // Clears the framebuffer to color immediately. Rectangles queued earlier
    // in the frame are discarded, since the clear would cover them anyway.
    void clear(const Color& color);

    // Queues a rectangle in logical units. Rectangles with a non-positive
    // size are ignored.
    void drawRect(const Rect& rect, const Color& color);

    // Anti-aliased rectangle with circular corners. The radius is clamped to
    // half the shorter side; a radius of zero matches drawRect.
    void drawRoundedRect(const Rect& rect, float radius, const Color& color);

    // Outline drawn inside rect, width logical units thick.
    void drawBorder(const Rect& rect, float width, const Color& color, float radius = 0);

    // Anti-aliased segment with round caps, width logical units thick.
    void drawLine(Point from, Point to, float width, const Color& color);

    // Stretches the whole image over dest, multiplying its texels by tint.
    // The renderer uploads each image once and keeps the texture until
    // releaseImage() or destruction.
    void drawImage(const std::shared_ptr<Image>& image, const Rect& dest,
                   const Color& tint = {1, 1, 1, 1});

    // Frees an image's texture. Rejected while the current frame still uses it.
    void releaseImage(const Image& image);

    // Intersects the current clip, in logical coordinates. popClip rejects underflow.
    void pushClip(const Rect& clip);
    void popClip();
    // origin is the top-left of the text line box, in logical units.
    void drawText(const std::shared_ptr<Font>& font, std::string_view text,
                  Point origin, float logicalSize, const Color& color);

    // Submits all rectangles and text queued since beginFrame().
    void endFrame();

private:
    struct Vertex {
        float x;
        float y;
        Color color;
        float u, v;
        // Position relative to the rect center, then half extents, corner
        // radius and border width. Zero radius and border mean a plain fill.
        float localX, localY;
        float halfWidth, halfHeight, radius, border;
    };

    enum class Mode : int { Solid, Glyph, Image };
    struct Shape { float radius, border; };
    struct Command {
        std::size_t first{0}, count{0};
        unsigned int texture{0};
        Mode mode{Mode::Solid};
        std::optional<Rect> clip;
    };
    struct TextCache;
    struct ImageCache;
    void queueQuad(const Rect& rect, const Color& color, unsigned int texture, const Rect& uv,
                   Mode mode = Mode::Solid, Shape shape = {});
    // Corners run clockwise from the top-left of the shape's own frame;
    // local positions are relative to the shape's center in that frame.
    struct Corner { Point position, local, uv; };
    void queueCorners(const Corner (&corners)[4], const Color& color, unsigned int texture, Mode mode,
                      Size half, Shape shape);
    detail::GLHandle program_;
    detail::GLHandle vertexArray_;
    detail::GLHandle vertexBuffer_;
    int logicalSizeLocation_{-1}, modeLocation_{-1};
    Viewport viewport_{};
    std::vector<Vertex> vertices_;
    std::vector<Command> commands_;
    std::vector<Rect> clips_;
    std::unique_ptr<TextCache> textCache_;
    std::unique_ptr<ImageCache> imageCache_;
};

} // namespace gui
