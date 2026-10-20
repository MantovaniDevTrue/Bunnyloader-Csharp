#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionSpawnSite.h"

#include <cassert>
#include <cstdint>
#include <vector>

using bl::csharp::InvasionSpawnSite;
using bl::csharp::InvasionTileView;
using bl::csharp::findInvasionSpawnSite;

struct TestWorld {
    static constexpr int width = 260;
    static constexpr int height = 210;
    std::vector<uint32_t> lookup = std::vector<uint32_t>(width * height);
    uint16_t type[3] = {0, 1, 2};
    int16_t headers[3] = {0, 0x20, 0x20};
    uint8_t solid[3] = {0, 1, 1};
    uint8_t solidTop[3] = {0, 0, 1};

    InvasionTileView view() const {
        return {width, height, lookup.data(), type, headers,
                solid, solidTop, 3, 3};
    }

    void fill(int startY, uint32_t definition) {
        for (int y = startY; y < height; ++y)
            for (int x = 0; x < width; ++x)
                lookup[static_cast<size_t>(y) * width + x] = definition;
    }

    void set(int x, int y, uint32_t definition) {
        lookup[static_cast<size_t>(y) * width + x] = definition;
    }
};

int main() {
    TestWorld world;
    world.fill(100, 1);
    auto placement = findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16);
    assert(placement.valid && placement.x == 90 * 16 &&
           placement.y == 100 * 16);

    world.set(90, 98, 1);
    placement = findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16);
    assert(placement.valid && placement.x == 82 * 16 &&
           placement.y == 100 * 16);

    world.set(90, 98, 0);
    world.set(89, 100, 0);
    placement = findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16);
    assert(placement.valid && placement.x == 82 * 16);
    world.set(89, 100, 1);

    world.fill(100, 2);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);

    world.fill(100, 1);
    for (int x = 42; x < width - 42; ++x)
        world.set(x, 97, 1);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);

    world.fill(100, 1);
    for (int x = 42; x < width - 42; ++x)
        world.set(x, 97, 0);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 40 * 16).valid);
    assert(!findInvasionSpawnSite(world.view(), 0, 80 * 16).valid);
    assert(!findInvasionSpawnSite(world.view(), -1, 80 * 16).valid);

    InvasionTileView bad = world.view();
    bad.solid = nullptr;
    assert(!findInvasionSpawnSite(bad, 90 * 16, 80 * 16).valid);
    bad = world.view();
    bad.solidCount = 1;
    assert(!findInvasionSpawnSite(bad, 90 * 16, 80 * 16).valid);

    world.set(90, 99, 1);
    placement = findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16);
    assert(placement.valid && placement.x == 82 * 16);
    return 0;
}
