#pragma once

namespace gui {

// Axis-aligned rectangle in GUI coordinates: (0, 0) is the top-left corner,
// +x points right and +y points down. Units are logical window coordinates.
struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

} // namespace gui
