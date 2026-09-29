#pragma once

#include "gui/Color.hpp"
#include "gui/Geometry.hpp"
#include "gui/Viewport.hpp"
#include "gui/detail/GLHandle.hpp"

#include <vector>

namespace gui {

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

    // Submits all rectangles queued since beginFrame().
    void endFrame();

private:
    struct Vertex {
        float x;
        float y;
        Color color;
    };

    detail::GLHandle program_;
    detail::GLHandle vertexArray_;
    detail::GLHandle vertexBuffer_;
    int logicalSizeLocation_{-1};
    Viewport viewport_{};
    std::vector<Vertex> vertices_;
};

} // namespace gui
