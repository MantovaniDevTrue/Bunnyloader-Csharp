#include "experiments/csharp/NativeRarityColorWire.h"
#include "experiments/csharp/NativeRarityPalette.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <string_view>
#include <thread>

int main() {
    uint32_t color = 0;
    assert(bl::csharp::decodeRarityColorWire("COLOR|200|215|230|255", &color));
    assert(color == 0xffc8d7e6u);
    assert(bl::csharp::decodeRarityColorWire("COLOR|0|0|0|0", &color));
    assert(color == 0u);
    assert(bl::csharp::decodeRarityColorWire("COLOR|255|255|255|255", &color));
    assert(color == 0xffffffffu);
    for (std::string_view invalid : {
        "", "COLOR", "COLOR|1|2|3", "COLOR|1|2|3|4|5",
        "COLOR|-1|2|3|4", "COLOR|256|2|3|4", "COLOR|1|2|3|NaN",
        "COLOR|1|2|3|4x", "COLOR|1|2||4", "COLOR|1|+2|3|4",
        "COLOR|1|2|3|4\n", "COLOR|1|2|3| 4", "COLOR|1|2|3|4|"
    }) assert(!bl::csharp::decodeRarityColorWire(invalid, &color));
    assert(!bl::csharp::decodeRarityColorWire("COLOR|1|2|3|4", nullptr));

    bl::csharp::NativeRarityPalette palette;
    assert(!palette.read(11, &color));
    assert(!palette.read(12, &color));
    assert(!palette.publish(11, 0u));
    assert(!palette.publish(4108, 0u));
    assert(palette.publish(12, 0xffc8d7e6u));
    assert(palette.publish(4107, 0xff112233u));
    assert(palette.read(12, &color) && color == 0xffc8d7e6u);
    assert(palette.read(4107, &color) && color == 0xff112233u);
    assert(palette.invalidate(12));
    assert(!palette.read(12, &color));
    assert(palette.publish(12, 0x01020304u));

    std::atomic<bool> running{true};
    std::atomic<int> reads{0};
    std::thread reader([&] {
        while (running.load(std::memory_order_relaxed)) {
            uint32_t next = 0;
            assert(palette.read(12, &next));
            assert(next == 0x01020304u || next == 0xaabbccddu);
            reads.fetch_add(1, std::memory_order_relaxed);
        }
    });
    for (int i = 0; i < 10000; ++i)
        assert(palette.publish(12, i & 1 ? 0x01020304u : 0xaabbccddu));
    running.store(false, std::memory_order_relaxed);
    reader.join();
    assert(reads.load(std::memory_order_relaxed) >= 0);
}
