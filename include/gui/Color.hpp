#pragma once

namespace gui {

// Straight (non-premultiplied) RGBA color with components in [0, 1].
struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

} // namespace gui
