#pragma once

#include "gui/Event.hpp"

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace gui {

// Main-thread FIFO owning its events. Popping returns a value, never a reference
// into storage. Slots are reused; only construction and capacity growth allocate.
class EventQueue {
    static_assert(std::is_nothrow_move_constructible_v<Event>);
public:
    EventQueue() : slots_(64) {}
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue(EventQueue&& other) noexcept
        : slots_(std::move(other.slots_)), head_(std::exchange(other.head_, 0)),
          size_(std::exchange(other.size_, 0)) { other.slots_.clear(); }
    EventQueue& operator=(EventQueue&& other) noexcept {
        if (this != &other) {
            slots_ = std::move(other.slots_);
            head_ = std::exchange(other.head_, 0);
            size_ = std::exchange(other.size_, 0);
            other.slots_.clear();
        }
        return *this;
    }

    // Grows geometrically without dropping/coalescing events. If growth fails,
    // all previously queued events remain intact.
    void push(Event event) {
        if (size_ == slots_.size()) grow();
        slots_[(head_ + size_) % slots_.size()].emplace(std::move(event));
        ++size_;
    }
    std::optional<Event> pop() noexcept {
        if (empty()) return std::nullopt;
        auto event = std::move(slots_[head_]);
        slots_[head_].reset();
        head_ = (head_ + 1) % slots_.size();
        if (--size_ == 0) head_ = 0;
        return event;
    }
    // Discards pending events but retains allocated slots.
    void clear() noexcept { while (pop()) {} }
    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return slots_.size(); }

private:
    void grow() {
        const auto oldCapacity = slots_.size();
        if (oldCapacity > slots_.max_size() / 2)
            throw std::length_error("Event queue capacity exceeded");
        std::vector<std::optional<Event>> replacement(oldCapacity ? oldCapacity * 2 : 64);
        for (std::size_t i = 0; i < size_; ++i)
            replacement[i] = slots_[(head_ + i) % oldCapacity];
        slots_.swap(replacement);
        head_ = 0;
    }
    std::vector<std::optional<Event>> slots_;
    std::size_t head_{0}, size_{0};
};

} // namespace gui
