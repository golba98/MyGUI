#pragma once

#include <utility>

namespace gui::detail {

// Owns a single OpenGL object name and deletes it on destruction. The deleter
// is supplied by the code that created the object, so this header needs no
// OpenGL includes. The owning context must still be current when it is destroyed.
class GLHandle {
public:
    using Deleter = void (*)(unsigned int id);

    GLHandle() = default;

    GLHandle(unsigned int id, Deleter deleter) noexcept
        : id_{id}, deleter_{deleter} {}

    ~GLHandle() {
        reset();
    }

    GLHandle(const GLHandle&) = delete;
    GLHandle& operator=(const GLHandle&) = delete;

    GLHandle(GLHandle&& other) noexcept
        : id_{std::exchange(other.id_, 0u)},
          deleter_{other.deleter_} {}

    GLHandle& operator=(GLHandle&& other) noexcept {
        if (this != &other) {
            reset();
            id_ = std::exchange(other.id_, 0u);
            deleter_ = other.deleter_;
        }

        return *this;
    }

    unsigned int get() const noexcept {
        return id_;
    }

private:
    void reset() noexcept {
        if (id_ != 0 && deleter_) {
            deleter_(id_);
        }

        id_ = 0;
    }

    unsigned int id_{0};
    Deleter deleter_{nullptr};
};

} // namespace gui::detail
