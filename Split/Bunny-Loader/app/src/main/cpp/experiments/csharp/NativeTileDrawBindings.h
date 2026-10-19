#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>

namespace bl::csharp::tile_draw {
template<typename T, std::size_t Capacity>
class PublishedBindings {
    std::array<std::shared_ptr<const T>, Capacity> bindings_{};
public:
    std::shared_ptr<const T> load(int index) const {
        if (index < 0 || static_cast<std::size_t>(index) >= Capacity) return {};
        return std::atomic_load_explicit(&bindings_[static_cast<std::size_t>(index)],
                                         std::memory_order_acquire);
    }

    bool publish(int index, const T& value) {
        if (index < 0 || static_cast<std::size_t>(index) >= Capacity) return false;
        std::shared_ptr<const T> next = std::make_shared<const T>(value);
        std::atomic_store_explicit(&bindings_[static_cast<std::size_t>(index)],
                                   next, std::memory_order_release);
        return true;
    }
};
}
