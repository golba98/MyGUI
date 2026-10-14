#include "gui/Renderer.hpp"

#include "gl/Functions.hpp"
#include "gui/Font.hpp"
#include "gui/Image.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace gui {

namespace {

static_assert(std::is_same_v<GLuint, unsigned int>, "GLHandle stores names as unsigned int");

constexpr std::size_t initialRectCapacity = 256;
constexpr std::size_t verticesPerRect = 6;

// Positions arrive in logical units (top-left origin, y down) and are mapped
// to normalized device coordinates (bottom-left origin, y up) here. The
// OpenGL viewport then stretches that range across the framebuffer's pixels,
// which is what applies the display scale.
constexpr const char* vertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec2 aLocal;
layout(location = 4) in vec4 aShape;

uniform vec2 uLogicalSize;

out vec4 vColor;
out vec2 vUV;
out vec2 vLocal;
flat out vec4 vShape;

void main() {
    vec2 ndc = aPosition / uLogicalSize * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
    vLocal = aLocal;
    vShape = aShape;
}
)";

constexpr const char* fragmentShaderSource = R"(#version 330 core
in vec4 vColor;
in vec2 vUV;
in vec2 vLocal;
flat in vec4 vShape;
uniform sampler2D uAtlas;
// 0: solid color, 1: glyph coverage in the red channel, 2: RGBA image.
uniform int uMode;

out vec4 fragColor;

// Signed distance to a rounded box centered at the origin; negative inside.
float roundedBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    vec4 color = vColor;
    if (uMode == 1) color.a *= texture(uAtlas, vUV).r;
    else if (uMode == 2) color *= texture(uAtlas, vUV);
    if (vShape.z > 0.0 || vShape.w > 0.0) {
        float distance = roundedBox(vLocal, vShape.xy, vShape.z);
        // One framebuffer pixel, in logical units, for anti-aliasing.
        float pixel = max(fwidth(distance), 1e-4);
        float coverage = clamp(0.5 - distance / pixel, 0.0, 1.0);
        if (vShape.w > 0.0) coverage *= clamp(0.5 + (distance + vShape.w) / pixel, 0.0, 1.0);
        color.a *= coverage;
    }
    fragColor = color;
}
)";

// Texture uploads must not inherit another caller's unpack layout or PBO.
class UnpackGuard {
public:
    UnpackGuard() {
        gl::GetIntegerv(GL_UNPACK_ALIGNMENT, &alignment_);
        gl::GetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength_);
        gl::GetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows_);
        gl::GetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels_);
        gl::GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer_);
        gl::BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        gl::PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        gl::PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
    ~UnpackGuard() {
        gl::PixelStorei(GL_UNPACK_ALIGNMENT, alignment_);
        gl::PixelStorei(GL_UNPACK_ROW_LENGTH, rowLength_);
        gl::PixelStorei(GL_UNPACK_SKIP_ROWS, skipRows_);
        gl::PixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels_);
        gl::BindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer_));
    }
    UnpackGuard(const UnpackGuard&) = delete;
    UnpackGuard& operator=(const UnpackGuard&) = delete;
private:
    GLint alignment_{}, rowLength_{}, skipRows_{}, skipPixels_{}, buffer_{};
};

void deleteShader(unsigned int id) {
    gl::DeleteShader(id);
}

void deleteProgram(unsigned int id) {
    gl::DeleteProgram(id);
}

void deleteVertexArray(unsigned int id) {
    gl::DeleteVertexArrays(1, &id);
}

void deleteTexture(unsigned int id) { gl::DeleteTextures(1, &id); }

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

struct Renderer::TextCache {
    struct Page {
        detail::GLHandle texture;
        int size{0}, x{1}, y{1}, rowHeight{0};
    };
    struct Glyph {
        unsigned int texture{0};
        Rect uv{};
        int width{0}, height{0}, left{0}, top{0};
    };
    using Key = std::tuple<Font*, char32_t, unsigned int, unsigned int>;
    std::map<Key, Glyph> glyphs;
    std::map<Font*, std::shared_ptr<Font>, std::less<>> fonts;
    std::vector<Page> pages;

    const Glyph& get(const std::shared_ptr<Font>& font, char32_t codepoint, unsigned int width, unsigned int height) {
        const Key key{font.get(), codepoint, width, height};
        if (const auto it = glyphs.find(key); it != glyphs.end()) return it->second;
        fonts.emplace(font.get(), font);
        const auto bitmap = font->rasterize(codepoint, width, height);
        Glyph glyph;
        glyph.width = bitmap.width; glyph.height = bitmap.height;
        glyph.left = bitmap.left; glyph.top = bitmap.top;
        if (bitmap.width > 0 && bitmap.height > 0) {
            int size = 1024;
            while (size < bitmap.width + 2 || size < bitmap.height + 2) size *= 2;
            GLint maxSize{};
            gl::GetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
            if (size > maxSize) throw std::runtime_error("Glyph exceeds the maximum texture size");
            if (!pages.empty()) {
                auto& page = pages.back();
                if (page.x + bitmap.width + 1 > page.size) {
                    page.x = 1; page.y += page.rowHeight + 1; page.rowHeight = 0;
                }
            }
            const UnpackGuard unpackGuard;
            gl::ActiveTexture(GL_TEXTURE0);
            if (pages.empty() || pages.back().size < size || pages.back().y + bitmap.height + 1 > pages.back().size) {
                GLuint texture{};
                gl::GenTextures(1, &texture);
                if (!texture) throw std::runtime_error("Failed to allocate glyph atlas");
                Page page{detail::GLHandle{texture, &deleteTexture}, size};
                gl::BindTexture(GL_TEXTURE_2D, texture);
                gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                const std::vector<unsigned char> zeroes(static_cast<std::size_t>(size) * size, 0);
                gl::TexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, zeroes.data());
                pages.push_back(std::move(page));
            }
            auto& page = pages.back();
            gl::BindTexture(GL_TEXTURE_2D, page.texture.get());
            gl::TexSubImage2D(GL_TEXTURE_2D, 0, page.x, page.y, bitmap.width, bitmap.height,
                             GL_RED, GL_UNSIGNED_BYTE, bitmap.pixels.data());
            gl::BindTexture(GL_TEXTURE_2D, 0);
            glyph.texture = page.texture.get();
            const float denominator = static_cast<float>(page.size);
            glyph.uv = {page.x / denominator, page.y / denominator,
                        bitmap.width / denominator, bitmap.height / denominator};
            page.x += bitmap.width + 1;
            page.rowHeight = std::max(page.rowHeight, bitmap.height);
        }
        return glyphs.emplace(key, glyph).first->second;
    }
};

struct Renderer::ImageCache {
    struct Entry {
        std::shared_ptr<Image> image;
        detail::GLHandle texture;
    };
    std::map<const Image*, Entry, std::less<>> entries;

    unsigned int get(const std::shared_ptr<Image>& image) {
        if (const auto it = entries.find(image.get()); it != entries.end()) return it->second.texture.get();
        GLint maxSize{};
        gl::GetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (image->width() > maxSize || image->height() > maxSize)
            throw std::runtime_error("Image exceeds the maximum texture size");
        GLuint id{};
        gl::GenTextures(1, &id);
        if (!id) throw std::runtime_error("Failed to allocate image texture");
        detail::GLHandle texture{id, &deleteTexture};
        const UnpackGuard unpackGuard;
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(GL_TEXTURE_2D, id);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width(), image->height(), 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, image->pixels().data());
        gl::BindTexture(GL_TEXTURE_2D, 0);
        entries.emplace(image.get(), Entry{image, std::move(texture)});
        return id;
    }
};

Renderer::Renderer(GLProcLoader loader) {
    gl::load(loader);

    const auto vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    const auto fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    program_ = linkProgram(vertexShader, fragmentShader);
    logicalSizeLocation_ = gl::GetUniformLocation(program_.get(), "uLogicalSize");
    modeLocation_ = gl::GetUniformLocation(program_.get(), "uMode");
    textCache_ = std::make_unique<TextCache>();
    imageCache_ = std::make_unique<ImageCache>();
    gl::UseProgram(program_.get());
    gl::Uniform1i(gl::GetUniformLocation(program_.get(), "uAtlas"), 0);
    gl::UseProgram(0);

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

    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, u)));
    gl::EnableVertexAttribArray(3);
    gl::VertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, localX)));
    gl::EnableVertexAttribArray(4);
    gl::VertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, halfWidth)));
    gl::BindVertexArray(0);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);

    vertices_.reserve(initialRectCapacity * verticesPerRect);
}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&& other) noexcept = default;

Renderer& Renderer::operator=(Renderer&& other) noexcept = default;

void Renderer::beginFrame(const Viewport& viewport) {
    viewport_ = viewport;

    // clear() keeps the capacity, so steady-state frames do not allocate.
    vertices_.clear(); commands_.clear(); clips_.clear();
    gl::Disable(GL_SCISSOR_TEST);

    gl::Viewport(0, 0, viewport.framebufferWidth, viewport.framebufferHeight);
}

void Renderer::clear(const Color& color) {
    vertices_.clear(); commands_.clear();
    gl::Disable(GL_SCISSOR_TEST);

    gl::ClearColor(color.r, color.g, color.b, color.a);
    gl::Clear(GL_COLOR_BUFFER_BIT);
}

void Renderer::pushClip(const Rect& clip) {
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.width) || !std::isfinite(clip.height))
        throw std::invalid_argument("Clip must contain finite coordinates");
    const Rect viewport{0, 0, static_cast<float>(std::max(0, viewport_.logicalWidth)),
                       static_cast<float>(std::max(0, viewport_.logicalHeight))};
    clips_.push_back(intersect(clips_.empty() ? viewport : clips_.back(), clip));
}
void Renderer::popClip() {
    if (clips_.empty()) throw std::logic_error("Clip stack underflow");
    clips_.pop_back();
}
void Renderer::queueQuad(const Rect& rect, const Color& color, unsigned int texture, const Rect& uv,
                         Mode mode, Shape shape) {
    if (rect.empty()) return;
    const float right = rect.x + rect.width, bottom = rect.y + rect.height;
    const float u1 = uv.x + uv.width, v1 = uv.y + uv.height;
    const float hw = rect.width / 2, hh = rect.height / 2;
    queueCorners({{{rect.x, rect.y}, {-hw, -hh}, {uv.x, uv.y}}, {{right, rect.y}, {hw, -hh}, {u1, uv.y}},
                  {{right, bottom}, {hw, hh}, {u1, v1}}, {{rect.x, bottom}, {-hw, hh}, {uv.x, v1}}},
                 color, texture, mode, {hw, hh}, shape);
}
void Renderer::queueCorners(const Corner (&corners)[4], const Color& color, unsigned int texture, Mode mode,
                            Size half, Shape shape) {
    if (!clips_.empty() && clips_.back().empty()) return;
    const auto clip = clips_.empty() ? std::optional<Rect>{} : clips_.back();
    const bool merge = !commands_.empty() && commands_.back().texture == texture
        && commands_.back().mode == mode && commands_.back().clip == clip;
    if (!merge && commands_.size() == commands_.capacity())
        commands_.reserve(std::max(commands_.size() + 1, commands_.capacity() * 2));
    if (vertices_.size() + verticesPerRect > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::length_error("Too many vertices in one frame");
    if (vertices_.size() + verticesPerRect > vertices_.capacity())
        vertices_.reserve(std::max(vertices_.size() + verticesPerRect, vertices_.capacity() * 2));
    if (!merge) commands_.push_back({vertices_.size(), 0, texture, mode, clip});
    const float limit = std::min(half.width, half.height);
    const float radius = std::min(shape.radius, limit), border = std::min(shape.border, limit);
    const auto vertex = [&](const Corner& c) {
        return Vertex{c.position.x, c.position.y, color, c.uv.x, c.uv.y, c.local.x, c.local.y,
                      half.width, half.height, radius, border};
    };
    for (const int i : {0, 1, 2, 0, 2, 3}) vertices_.push_back(vertex(corners[i]));
    commands_.back().count += verticesPerRect;
}
void Renderer::drawRect(const Rect& rect, const Color& color) {
    if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.x + rect.width)
        || !std::isfinite(rect.y + rect.height)) throw std::invalid_argument("Rectangle must contain finite coordinates");
    queueQuad(rect, color, 0, {});
}
void Renderer::drawRoundedRect(const Rect& rect, float radius, const Color& color) {
    if (!std::isfinite(radius)) throw std::invalid_argument("Corner radius must be finite");
    if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.x + rect.width)
        || !std::isfinite(rect.y + rect.height)) throw std::invalid_argument("Rectangle must contain finite coordinates");
    queueQuad(rect, color, 0, {}, Mode::Solid, {nonnegative(radius), 0});
}
void Renderer::drawBorder(const Rect& rect, float width, const Color& color, float radius) {
    if (!std::isfinite(width) || !std::isfinite(radius)) throw std::invalid_argument("Border width and radius must be finite");
    if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.x + rect.width)
        || !std::isfinite(rect.y + rect.height)) throw std::invalid_argument("Rectangle must contain finite coordinates");
    if (width <= 0) return;
    queueQuad(rect, color, 0, {}, Mode::Solid, {nonnegative(radius), width});
}
void Renderer::drawLine(Point from, Point to, float width, const Color& color) {
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y)
        || !std::isfinite(width)) throw std::invalid_argument("Line must contain finite coordinates and width");
    if (width <= 0) return;
    const float dx = to.x - from.x, dy = to.y - from.y, length = std::hypot(dx, dy);
    // A degenerate segment is a dot; any direction works.
    const float ux = length > 0 ? dx / length : 1, uy = length > 0 ? dy / length : 0;
    const float r = width / 2, hl = length / 2 + r;
    // Rotated capsule: extend by the cap radius along the segment and to both sides.
    const Point center{(from.x + to.x) / 2, (from.y + to.y) / 2};
    const auto at = [&](float along, float across) {
        return Point{center.x + ux * along - uy * across, center.y + uy * along + ux * across};
    };
    queueCorners({{at(-hl, -r), {-hl, -r}, {}}, {at(hl, -r), {hl, -r}, {}},
                  {at(hl, r), {hl, r}, {}}, {at(-hl, r), {-hl, r}, {}}},
                 color, 0, Mode::Solid, {hl, r}, {r, 0});
}
void Renderer::drawImage(const std::shared_ptr<Image>& image, const Rect& dest, const Color& tint) {
    if (!image) throw std::invalid_argument("drawImage requires an image");
    if (!std::isfinite(dest.x) || !std::isfinite(dest.y) || !std::isfinite(dest.x + dest.width)
        || !std::isfinite(dest.y + dest.height)) throw std::invalid_argument("Image destination must contain finite coordinates");
    if (dest.empty() || (!clips_.empty() && clips_.back().empty())) return;
    queueQuad(dest, tint, imageCache_->get(image), {0, 0, 1, 1}, Mode::Image);
}
void Renderer::releaseImage(const Image& image) {
    const auto it = imageCache_->entries.find(&image);
    if (it == imageCache_->entries.end()) return;
    const auto texture = it->second.texture.get();
    if (std::any_of(commands_.begin(), commands_.end(), [texture](const Command& c) { return c.texture == texture; }))
        throw std::logic_error("Cannot release an image queued in the current frame");
    imageCache_->entries.erase(it);
}
void Renderer::drawText(const std::shared_ptr<Font>& font, std::string_view text,
                        Point origin, float logicalSize, const Color& color) {
    if (!font) throw std::invalid_argument("Text requires a font");
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) throw std::invalid_argument("Text origin must be finite");
    std::vector<Font::PositionedGlyph> positioned;
    font->shape(text, logicalSize, &positioned);
    if (viewport_.logicalWidth <= 0 || viewport_.logicalHeight <= 0
        || viewport_.framebufferWidth <= 0 || viewport_.framebufferHeight <= 0
        || (!clips_.empty() && clips_.back().empty())) return;
    const double requestedX = std::ceil(static_cast<double>(logicalSize) * viewport_.scaleX());
    const double requestedY = std::ceil(static_cast<double>(logicalSize) * viewport_.scaleY());
    if (requestedX > 4096 || requestedY > 4096) throw std::invalid_argument("Scaled font size exceeds 4096 pixels");
    const auto width = static_cast<unsigned int>(std::max(1.0, requestedX));
    const auto height = static_cast<unsigned int>(std::max(1.0, requestedY));
    const float scaleX = static_cast<float>(width) / logicalSize, scaleY = static_cast<float>(height) / logicalSize;
    for (const auto& positionedGlyph : positioned) {
        const auto& glyph = textCache_->get(font, positionedGlyph.codepoint, width, height);
        if (!glyph.texture) continue;
        queueQuad({origin.x + positionedGlyph.x + glyph.left / scaleX,
                   origin.y + positionedGlyph.baseline - glyph.top / scaleY,
                   glyph.width / scaleX, glyph.height / scaleY}, color, glyph.texture, glyph.uv, Mode::Glyph);
    }
}
void Renderer::endFrame() {
    if (!clips_.empty()) throw std::logic_error("Unbalanced clip stack at endFrame");
    const bool empty = viewport_.logicalWidth <= 0 || viewport_.logicalHeight <= 0
        || viewport_.framebufferWidth <= 0 || viewport_.framebufferHeight <= 0;
    if (vertices_.empty() || empty) { vertices_.clear(); commands_.clear(); return; }
    gl::Enable(GL_BLEND);
    gl::BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl::Disable(GL_DEPTH_TEST);
    gl::Disable(GL_CULL_FACE);
    gl::UseProgram(program_.get());
    gl::Uniform2f(logicalSizeLocation_, static_cast<float>(viewport_.logicalWidth), static_cast<float>(viewport_.logicalHeight));
    gl::BindVertexArray(vertexArray_.get());
    gl::BindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.get());
    gl::BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);
    gl::ActiveTexture(GL_TEXTURE0);
    for (const auto& command : commands_) {
        if (command.clip) {
            const auto& clip = *command.clip;
            const auto edge = [](double value, int limit, bool end) {
                return static_cast<int>(std::clamp(end ? std::ceil(value) : std::floor(value), 0.0, static_cast<double>(limit)));
            };
            const int left = edge(clip.x * static_cast<double>(viewport_.scaleX()), viewport_.framebufferWidth, false);
            const int right = edge((clip.x + clip.width) * static_cast<double>(viewport_.scaleX()), viewport_.framebufferWidth, true);
            const int top = edge(clip.y * static_cast<double>(viewport_.scaleY()), viewport_.framebufferHeight, false);
            const int bottom = edge((clip.y + clip.height) * static_cast<double>(viewport_.scaleY()), viewport_.framebufferHeight, true);
            gl::Enable(GL_SCISSOR_TEST);
            gl::Scissor(left, viewport_.framebufferHeight - bottom, std::max(0, right - left), std::max(0, bottom - top));
        }
        else gl::Disable(GL_SCISSOR_TEST);
        gl::Uniform1i(modeLocation_, static_cast<int>(command.mode));
        gl::BindTexture(GL_TEXTURE_2D, command.texture);
        gl::DrawArrays(GL_TRIANGLES, static_cast<GLint>(command.first), static_cast<GLsizei>(command.count));
    }
    gl::Disable(GL_SCISSOR_TEST);
    gl::BindTexture(GL_TEXTURE_2D, 0);
    gl::BindVertexArray(0);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);
    gl::UseProgram(0);
    vertices_.clear(); commands_.clear();
}

} // namespace gui
