#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace bl::csharp::hair {

// Reserva faixas CONTÍGUAS de IDs na thread de registro do jogo.
// O callback occupied(type) lê tanto PlayerHair quanto PlayerHairAlt e
// quaisquer IDs previamente reservados pelo Bunny JS e outros loaders.
// NÃO chamar por frame nem reservar em paralelo: isto é estado de startup.
class TypeReservations {
public:
    using Occupied = std::function<bool(int type)>;

    struct Block {
        int first = -1;
        int count = 0;
    };

    explicit TypeReservations(int minimum = Registry::kVanillaCount,
                              int maximum = Registry::kLastType)
        : minimum_(minimum), maximum_(maximum) {}

    bool Reserve(std::string_view uid, int count,
                 const Occupied& occupied, Block* result, std::string* error) {
        if (!result || !error) return false;
        *result = {};
        error->clear();
        if (!validIdentifier(uid) || !occupied ||
            minimum_ < Registry::kVanillaCount || maximum_ > Registry::kLastType ||
            maximum_ < minimum_ || count <= 0 || count > 2048) {
            *error = "HairReservationInvalidArguments";
            return false;
        }
        const auto existing = byMod_.find(std::string(uid));
        if (existing != byMod_.end()) {
            if (existing->second.count != count) {
                *error = "HairReservationModCountChanged";
                return false;
            }
            *result = existing->second;
            return true;
        }
        const int finalStart = maximum_ - count + 1;
        for (int first = minimum_; first <= finalStart; ++first) {
            bool free = true;
            for (int delta = 0; delta < count; ++delta) {
                const int id = first + delta;
                if (reserved_.contains(id) || occupied(id)) {
                    free = false;
                    first = id; // a próxima tentativa inicia após o slot ocupado
                    break;
                }
            }
            if (!free) continue;
            for (int delta = 0; delta < count; ++delta)
                reserved_.insert(first + delta);
            Block block{first, count};
            byMod_.emplace(std::string(uid), block);
            *result = block;
            return true;
        }
        *error = "HairReservationNoContiguousRange";
        return false;
    }

    bool Release(std::string_view uid) {
        const auto found = byMod_.find(std::string(uid));
        if (found == byMod_.end()) return false;
        const auto block = found->second;
        for (int delta = 0; delta < block.count; ++delta)
            reserved_.erase(block.first + delta);
        byMod_.erase(found);
        return true;
    }

    bool Contains(int type) const { return reserved_.contains(type); }

    Block ForMod(std::string_view uid) const {
        const auto found = byMod_.find(std::string(uid));
        return found == byMod_.end() ? Block{} : found->second;
    }

    size_t ModCount() const { return byMod_.size(); }

    void Reset() {
        byMod_.clear();
        reserved_.clear();
    }

private:
    int minimum_;
    int maximum_;
    std::map<std::string, Block, std::less<>> byMod_;
    std::set<int> reserved_;
};

} // namespace bl::csharp::hair
