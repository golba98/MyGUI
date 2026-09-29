#include "gui/Renderer.hpp"

#include "gl/Functions.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace gui {

namespace {

static_assert(std::is_same_v<GLuint, unsigned int>, "GLHandle stores names as unsigned int");

constexpr std::size_t initialRectCapacity = 256;
constexpr std::size_t verticesPerRect = 6;

// Positions arrive in GUI pixels (top-left origin, y down) and are mapped to
// normalized device coordinates (bottom-left origin, y up) here.
constexpr const char* vertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

uniform vec2 uViewportSize;

out vec4 vColor;

void main() {
    vec2 ndc = aPosition / uViewportSize * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr const char* fragmentShaderSource = R"(#version 330 core
in vec4 vColor;

out vec4 fragColor;

void main() {
    fragColor = vColor;
}
)";

void deleteShader(unsigned int id) {
    gl::DeleteShader(id);
}

void deleteProgram(unsigned int id) {
    gl::DeleteProgram(id);
}

void deleteVertexArray(unsigned int id) {
    gl::DeleteVertexArrays(1, &id);
}

void deleteBuffer(unsigned int id) {
    gl::DeleteBuffers(1, &id);
}

detail::GLHandle compileShader(GLenum type, const char* source) {
    detail::GLHandle shader{gl::CreateShader(type), &deleteShader};

    gl::ShaderSource(shader.get(), 1, &source, nullptr);
    gl::CompileShader(shader.get());

    GLint compiled{GL_FALSE};
    gl::GetShaderiv(shader.get(), GL_COMPILE_STATUS, &compiled);

    if (compiled != GL_TRUE) {
        GLint length{0};
        gl::GetShaderiv(shader.get(), GL_INFO_LOG_LENGTH, &length);

        std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
        gl::GetShaderInfoLog(shader.get(), static_cast<GLsizei>(log.size()), nullptr, log.data());

        throw std::runtime_error("Failed to compile shader: " + log);
    }

    return shader;
}

detail::GLHandle linkProgram(const detail::GLHandle& vertexShader, const detail::GLHandle& fragmentShader) {
    detail::GLHandle program{gl::CreateProgram(), &deleteProgram};

    gl::AttachShader(program.get(), vertexShader.get());
    gl::AttachShader(program.get(), fragmentShader.get());
    gl::LinkProgram(program.get());

    // The linked program keeps its own copy; detaching lets the shaders be freed.
    gl::DetachShader(program.get(), vertexShader.get());
    gl::DetachShader(program.get(), fragmentShader.get());

    GLint linked{GL_FALSE};
    gl::GetProgramiv(program.get(), GL_LINK_STATUS, &linked);

    if (linked != GL_TRUE) {
        GLint length{0};
        gl::GetProgramiv(program.get(), GL_INFO_LOG_LENGTH, &length);

        std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
        gl::GetProgramInfoLog(program.get(), static_cast<GLsizei>(log.size()), nullptr, log.data());

        throw std::runtime_error("Failed to link shader program: " + log);
    }

    return program;
}

} // namespace

Renderer::Renderer(GLProcLoader loader) {
    gl::load(loader);

    const auto vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    const auto fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    program_ = linkProgram(vertexShader, fragmentShader);
    viewportSizeLocation_ = gl::GetUniformLocation(program_.get(), "uViewportSize");

    GLuint vertexArray{0};
    gl::GenVertexArrays(1, &vertexArray);
    vertexArray_ = detail::GLHandle{vertexArray, &deleteVertexArray};

    GLuint vertexBuffer{0};
    gl::GenBuffers(1, &vertexBuffer);
    vertexBuffer_ = detail::GLHandle{vertexBuffer, &deleteBuffer};

    // The vertex array records the buffer binding and attribute layout.
    gl::BindVertexArray(vertexArray_.get());
    gl::BindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.get());

    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, x))
    );

    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(
        1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, color))
    );

    gl::BindVertexArray(0);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);

    vertices_.reserve(initialRectCapacity * verticesPerRect);
}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&& other) noexcept = default;

Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

void Renderer::beginFrame(int width, int height) {
    width_ = width;
    height_ = height;

    // clear() keeps the capacity, so steady-state frames do not allocate.
    vertices_.clear();

    gl::Viewport(0, 0, width, height);
}

void Renderer::drawRect(const Rect& rect, const Color& color) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }

    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.width;
    const float bottom = rect.y + rect.height;

    vertices_.push_back({left, top, color});
    vertices_.push_back({right, top, color});
    vertices_.push_back({right, bottom, color});

    vertices_.push_back({left, top, color});
    vertices_.push_back({right, bottom, color});
    vertices_.push_back({left, bottom, color});
}

void Renderer::endFrame() {
    // Nothing queued, or the window is minimized.
    if (vertices_.empty() || width_ <= 0 || height_ <= 0) {
        return;
    }

    // Straight alpha for color; destination alpha accumulates coverage.
    gl::Enable(GL_BLEND);
    gl::BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl::Disable(GL_DEPTH_TEST);

    gl::UseProgram(program_.get());
    gl::Uniform2f(viewportSizeLocation_, static_cast<float>(width_), static_cast<float>(height_));

    gl::BindVertexArray(vertexArray_.get());
    gl::BindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.get());
    gl::BufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
        vertices_.data(),
        GL_STREAM_DRAW
    );

    gl::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));

    gl::BindVertexArray(0);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);
    gl::UseProgram(0);

    vertices_.clear();
}

} // namespace gui
