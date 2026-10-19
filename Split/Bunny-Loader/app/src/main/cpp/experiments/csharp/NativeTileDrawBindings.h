#pragma once
#include <array>
#include <mutex>
#include <shared_mutex>
#include <cstddef>
#include <memory>

namespace bl::csharp::tile_draw {
template<typename T, std::size_t Capacity>
class PublishedBindings {
    std::array<std::shared_ptr<const T>, Capacity> bindings_{};
    mutable std::shared_mutex mutex_;
public:
    std::shared_ptr<const T> load(int index) const {
        if (index < 0 || static_cast<std::size_t>(index) >= Capacity) return {};
        std::shared_lock lock(mutex_);
        return bindings_[static_cast<std::size_t>(index)];
    }

    bool publish(int index, const T& value) {
        if (index < 0 || static_cast<std::size_t>(index) >= Capacity) return false;
        std::shared_ptr<const T> next = std::make_shared<const T>(value);
        std::unique_lock lock(mutex_);
        bindings_[static_cast<std::size_t>(index)] = std::move(next);
        return true;
    }
};
}
