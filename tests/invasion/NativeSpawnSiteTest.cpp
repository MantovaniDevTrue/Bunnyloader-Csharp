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
    uint16_t type[6] = {0, 1, 2, 232, 1, 1};
    int16_t headers[6] = {0, 0x20, 0x20, 0x20, 0x1020, 0x0420};
    std::vector<uint8_t> solid = std::vector<uint8_t>(233, 0);
    std::vector<uint8_t> solidTop = std::vector<uint8_t>(233, 0);

    TestWorld() {
        solid[1] = 1;
        solid[2] = 1;
        solid[232] = 1;
        solidTop[2] = 1;
    }

    InvasionTileView view() const {
        return {width, height, lookup.data(), type, headers,
                solid.data(), solidTop.data(), solid.size(), solidTop.size()};
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

    world.fill(42, 1);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);

    for (int y = 42; y < 100; ++y)
        for (int x = 0; x < TestWorld::width; ++x)
            world.set(x, y, 0);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 40 * 16).valid);
    assert(!findInvasionSpawnSite(world.view(), 0, 80 * 16).valid);
    assert(!findInvasionSpawnSite(world.view(), -1, 80 * 16).valid);

    InvasionTileView bad = world.view();
    bad.solid = nullptr;
    assert(!findInvasionSpawnSite(bad, 90 * 16, 80 * 16).valid);
    bad = world.view();
    bad.solidCount = 1;
    assert(!findInvasionSpawnSite(bad, 90 * 16, 80 * 16).valid);

    world.fill(100, 4);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);
    world.fill(100, 5);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);
    world.fill(100, 3);
    assert(!findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16).valid);
    world.fill(100, 1);

    world.set(90, 99, 1);
    placement = findInvasionSpawnSite(world.view(), 90 * 16, 80 * 16);
    assert(placement.valid && placement.x == 82 * 16);

    world.set(90, 99, 0);
    int tested = 0;
    placement = bl::csharp::findInvasionSpawnSiteChecked(
        world.view(), 90 * 16, 80 * 16,
        [&](int x, int) {
            ++tested;
            return x != 90 * 16;
        });
    assert(placement.valid && placement.x == 82 * 16 && tested >= 2);
    placement = bl::csharp::findInvasionSpawnSiteChecked(
        world.view(), 90 * 16, 80 * 16,
        [](int, int) { return false; });
    assert(!placement.valid);
    return 0;
}
