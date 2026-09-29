#pragma once

namespace gui {

// Size of the drawable area in two coordinate spaces.
//
// Logical units are used by widgets, mouse input and layout. Framebuffer
// units are physical pixels, used for the OpenGL viewport. They differ when
// the display is scaled (HiDPI).
struct Viewport {
    int logicalWidth{0};
    int logicalHeight{0};
    int framebufferWidth{0};
    int framebufferHeight{0};

    // Framebuffer pixels per logical unit, or 1 if the logical size is empty.
    float scaleX() const noexcept {
        return logicalWidth > 0
            ? static_cast<float>(framebufferWidth) / static_cast<float>(logicalWidth)
            : 1.0f;
    }

    float scaleY() const noexcept {
        return logicalHeight > 0
            ? static_cast<float>(framebufferHeight) / static_cast<float>(logicalHeight)
            : 1.0f;
    }
};

} // namespace gui
