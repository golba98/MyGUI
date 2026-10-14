#pragma once

#include "gui/Color.hpp"

namespace gui {
// Shared colors and metrics read by the built-in widgets while drawing.
// The defaults reproduce the original button palette.
struct Theme {
    Color background{0.07f, 0.08f, 0.11f, 1};
    Color surface{0.13f, 0.16f, 0.21f, 1};
    Color accent{0.20f, 0.32f, 0.50f, 1};
    Color accentHovered{0.26f, 0.43f, 0.66f, 1};
    Color accentPressed{0.12f, 0.25f, 0.42f, 1};
    // Value indicators: checked boxes, slider and progress fills.
    Color fill{0.36f, 0.62f, 0.95f, 1};
    Color track{0.20f, 0.23f, 0.29f, 1};
    Color border{0.36f, 0.40f, 0.48f, 1};
    Color text{0.95f, 0.96f, 0.98f, 1};
    Color mutedText{0.62f, 0.66f, 0.72f, 1};
    Color disabled{0.22f, 0.24f, 0.28f, 1};
    Color disabledText{0.48f, 0.50f, 0.54f, 1};
    Color focus{0.60f, 0.80f, 1.0f, 1};
    Color selection{0.36f, 0.62f, 0.95f, 0.38f};
    float cornerRadius{6};
    float focusWidth{2};
};
} // namespace gui
