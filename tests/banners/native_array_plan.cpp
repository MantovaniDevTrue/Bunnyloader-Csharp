#include "experiments/csharp/ManagedBannerArrayBridge.h"

#include <cassert>
#include <iostream>
#include <string>

using namespace bl::csharp::banners;

int main() {
    Snapshot empty;
    assert(requiredArraySlots(empty, 697) == 697);
    assert(requiredArraySlots(empty, 700) == 700);
    assert(requiredArraySlots(empty, 262145) == -1);

    empty.bannerToItem[697] = 2100;
    assert(requiredArraySlots(empty, 697) == 698);
    assert(requiredArraySlots(empty, 701) == 701);
    empty.bannerToItem[262143] = 2101;
    assert(requiredArraySlots(empty, 700) == 262144);
    empty.bannerToItem[262144] = 2102;
    assert(requiredArraySlots(empty, 700) == -1);

    std::cout << "Banner arrays planner PASS (frame length, banner count, bounds)\n";
}
