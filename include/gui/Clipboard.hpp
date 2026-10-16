#pragma once

#include <string>
#include <string_view>

namespace gui {
// System or test clipboard holding UTF-8 text. Window::clipboard() provides
// the platform implementation; connect one with UIContext::setClipboard().
class Clipboard {
public:
    virtual ~Clipboard() = default;
    // Empty when the clipboard holds no text.
    virtual std::string text() const = 0;
    virtual void setText(std::string_view text) = 0;
};
} // namespace gui
