#pragma once

#include <algorithm>
#include <cmath>

namespace gui {

struct Point { float x{0}, y{0}; };
struct Size { float width{0}, height{0}; };
struct Insets { float left{0}, top{0}, right{0}, bottom{0}; };

// Logical window coordinates, with the origin at the top left.
struct Rect {
    float x{0}, y{0}, width{0}, height{0};
    bool contains(double px, double py) const noexcept {
        return width > 0 && height > 0 && px >= x && py >= y
            && px < x + width && py < y + height;
    }
    bool operator==(const Rect&) const = default;
    bool empty() const noexcept { return width <= 0 || height <= 0; }
};

inline Rect intersect(const Rect& a, const Rect& b) noexcept {
    const float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0.0f, std::min(a.x + a.width, b.x + b.width) - x),
                  std::max(0.0f, std::min(a.y + a.height, b.y + b.height) - y)};
}

inline float nonnegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

} // namespace gui
