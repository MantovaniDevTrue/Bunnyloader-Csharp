#include "experiments/csharp/ManagedBannerSaveSlice.h"

#include <cassert>
#include <iostream>

int main() {
    using bl::csharp::banners::vanillaBannerSaveRange;
    assert(vanillaBannerSaveRange(267,267,267));
    assert(vanillaBannerSaveRange(267,1024,1024));
    assert(vanillaBannerSaveRange(267,262144,262144));
    assert(!vanillaBannerSaveRange(0,1024,1024));
    assert(!vanillaBannerSaveRange(-1,1024,1024));
    assert(!vanillaBannerSaveRange(4097,4097,4097));
    assert(!vanillaBannerSaveRange(267,266,1024));
    assert(!vanillaBannerSaveRange(267,1024,266));
    assert(!vanillaBannerSaveRange(267,262145,267));
    assert(!vanillaBannerSaveRange(267,267,262145));
    std::cout << "Banner vanilla world save bounds PASS\n";
}
