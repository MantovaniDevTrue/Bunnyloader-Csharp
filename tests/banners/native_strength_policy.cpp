#include "experiments/csharp/ManagedBannerStrength.h"

#include <cassert>
#include <iostream>

int main() {
    using bl::csharp::banners::strengthItemSlotAllowed;
    constexpr int first = bl::runtime::kVanillaItemCount;
    assert(!strengthItemSlotAllowed(0, 9000));
    assert(!strengthItemSlotAllowed(first - 1, 9000));
    assert(!strengthItemSlotAllowed(first, first));
    assert(strengthItemSlotAllowed(first, first + 1));
    assert(strengthItemSlotAllowed(first + 1, 9000));
    assert(!strengthItemSlotAllowed(8999, 8999));
    assert(!strengthItemSlotAllowed(262144, 270000));
    assert(strengthItemSlotAllowed(262143, 262144));
    std::cout << "BannerStrength policy PASS (mod-only IDs, array bounds, vanilla unchanged)\n";
}
