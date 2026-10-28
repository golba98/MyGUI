#pragma once
#include <cstdint>
#include <memory>

namespace gui {
class UIContext;
// Copyable, non-owning identity. Dropping a handle does not close its overlay.
class OverlayHandle {
    friend class UIContext;
    std::weak_ptr<int> owner_;
    std::uint64_t id_{};
};
struct OverlayOptions { bool modal{false}; };
} // namespace gui
