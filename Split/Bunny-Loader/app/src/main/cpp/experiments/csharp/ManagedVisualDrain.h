#pragma once

#include "experiments/csharp/ManagedVisualIngress.h"
#include <cstddef>
#include <string>

namespace bl::csharp {

struct ManagedVisualDrainStats {
    size_t accepted = 0;
    size_t rejected = 0;
    size_t exhausted = 0;
};

class ManagedVisualDrain {
public:
    static constexpr size_t MaxPerUpdate = 64;

    template <typename ManagedNext>
    static ManagedVisualDrainStats poll(ManagedVisualQueue& destination,
                                        ManagedNext next, size_t budget = MaxPerUpdate) {
        ManagedVisualDrainStats stats;
        if (budget > MaxPerUpdate) budget = MaxPerUpdate;
        for (size_t i = 0; i < budget; ++i) {
            std::string command;
            if (!next(command) || command.empty()) {
                ++stats.exhausted;
                break;
            }
            if (ManagedVisualWire::accept(destination, command))
                ++stats.accepted;
            else
                ++stats.rejected;
        }
        return stats;
    }
};

}
