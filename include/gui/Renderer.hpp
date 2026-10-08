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

// Returns the address of an OpenGL function, or nullptr if it is unavailable.
// glfwGetProcAddress has exactly this signature.
using GLProc = void (*)();
using GLProcLoader = GLProc (*)(const char* name);

// Batched 2D renderer for solid-colored rectangles using OpenGL 3.3 core.
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
    };

    struct Command {
        std::size_t first{0}, count{0};
        unsigned int texture{0};
        std::optional<Rect> clip;
    };
    struct TextCache;
    void queueQuad(const Rect& rect, const Color& color, unsigned int texture, const Rect& uv);
    detail::GLHandle program_;
    detail::GLHandle vertexArray_;
    detail::GLHandle vertexBuffer_;
    int logicalSizeLocation_{-1}, texturedLocation_{-1};
    Viewport viewport_{};
    std::vector<Vertex> vertices_;
    std::vector<Command> commands_;
    std::vector<Rect> clips_;
    std::unique_ptr<TextCache> textCache_;
};

} // namespace gui
