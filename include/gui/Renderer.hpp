#pragma once

#include "gui/Color.hpp"
#include "gui/Geometry.hpp"
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
// order, so later rectangles are blended over earlier ones.
class Renderer {
public:
    explicit Renderer(GLProcLoader loader);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    // Starts a frame for a framebuffer of the given size in pixels.
    void beginFrame(int width, int height);

    // Queues a rectangle. Rectangles with a non-positive size are ignored.
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
    int viewportSizeLocation_{-1};
    int width_{0};
    int height_{0};
    std::vector<Vertex> vertices_;
};

} // namespace gui
