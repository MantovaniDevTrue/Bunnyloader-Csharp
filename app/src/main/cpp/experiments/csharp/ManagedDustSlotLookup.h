#pragma once

#include <array>
#include <cstddef>
#include <unordered_map>

namespace bl::csharp {

template <typename T, std::size_t Slots>
class ManagedDustSlotLookup {
public:
    int resolve(T* const* slots, std::size_t count, T* dust, int hint = -1) {
        if (!slots || !dust || count < Slots) return -1;
        if (slots != knownArray_) rebuild(slots);
        if (hint >= 0 && static_cast<std::size_t>(hint) < Slots &&
            slots[hint] == dust) return hint;
        auto found = positions_.find(dust);
        if (found != positions_.end() && slots[found->second] == dust)
            return static_cast<int>(found->second);
        rebuild(slots);
        found = positions_.find(dust);
        return found == positions_.end() ? -1 : static_cast<int>(found->second);
    }

    void reset() {
        knownArray_ = nullptr;
        positions_.clear();
    }

    std::size_t knownCount() const { return positions_.size(); }

private:
    void rebuild(T* const* slots) {
        knownArray_ = slots;
        positions_.clear();
        positions_.reserve(Slots);
        for (std::size_t i = 0; i < Slots; ++i)
            if (slots[i]) positions_[slots[i]] = i;
    }

    T* const* knownArray_ = nullptr;
    std::unordered_map<T*, std::size_t> positions_;
};

}
