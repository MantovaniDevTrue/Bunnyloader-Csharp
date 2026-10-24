#include "experiments/csharp/BunnyItemRarityId.h"
#include <cassert>

int main() {
    using namespace bl::csharp;
    int value = 1234;
    static_assert(kBunnyItemRarityIds.size() == 16);
    for (const auto& [name, expected] : kBunnyItemRarityIds) {
        assert(resolveBunnyItemRarity(name, &value));
        assert(value == expected);
    }
    assert(resolveBunnyItemRarity("Cyan", &value) && value == 9);
    assert(!resolveBunnyItemRarity("Count", &value));
    assert(!resolveBunnyItemRarity("Cyan ", &value));
    assert(!resolveBunnyItemRarity("", &value));
    assert(!resolveBunnyItemRarity("cyan", &value));
    assert(!resolveBunnyItemRarity("Cyan", nullptr));
    return 0;
}
