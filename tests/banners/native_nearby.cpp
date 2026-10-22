#include "experiments/csharp/ManagedBannerNearbyPolicy.h"

#include <cassert>
#include <iostream>

using bl::csharp::banners::NearbyBannerCache;

int main() {
    NearbyBannerCache cache;
    cache.beginWorld(31);
    assert(!cache.observe(30, 1, 4, 7, 754, 1, 697));
    assert(!cache.observe(31, 1, 4, 7, 753, 1, 697));
    assert(!cache.observe(31, 1, 4, 7, 754, -1, 697));
    assert(!cache.observe(31, 1, 4, 7, 754, 1, 696));
    assert(cache.observe(31, 1, 4, 7, 754, 1, 697));
    assert(cache.observe(31, 1, 7, 9, 755, 2, 701));
    assert(cache.observe(31, 2, 4, 7, 754, 1, 697));
    assert(cache.size() == 2);
    auto entries = cache.near(31, 2, 0, 0, 5, 8);
    assert(entries.size() == 1 && entries[0].banner == 697);
    entries = cache.near(31, 2, 0, 0, 10, 15);
    assert(entries.size() == 2);
    assert(cache.near(30, 2, 0, 0, 10, 15).empty());
    assert(cache.near(31, 2, 6, 0, 2, 8).empty());
    assert(cache.near(31, 2, 0, 0, 0, 8).empty());
    assert(cache.near(31, 2, 0, 0, 300, 8).empty());
    assert(cache.near(31, 200, 0, 0, 10, 15).empty());
    cache.prune(200);
    assert(cache.size() == 0);

    // Nunca carregar o buff do mundo anterior, mesmo se ID/relevo coincidirem.
    assert(cache.observe(31, 201, 4, 7, 754, 1, 697));
    cache.beginWorld(32);
    assert(cache.near(32, 201, 0, 0, 10, 15).empty());
    assert(!cache.observe(31, 201, 4, 7, 754, 1, 697));
    assert(cache.observe(32, 202, 4, 7, 754, 1, 697));
    cache.invalidate(4, 7);
    assert(cache.near(32, 202, 0, 0, 10, 15).empty());

    // 512 amostras no maximo: limites fixos para FPS e memoria.
    for (int i = 0; i < 600; ++i)
        assert(cache.observe(32, 203, i, 0, 754, 1, 697));
    assert(cache.size() == NearbyBannerCache::kMaximumEntries);
    cache.reset();
    assert(cache.size() == 0);
    std::cout << "Banner nearby cache PASS (bounds, world ownership, expiry, invalidation)\n";
}
