#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include "experiments/csharp/SourceParser.h"

#include <atomic>
#include <cassert>
#include <string>
#include <thread>
#include <vector>

int main() {
    bl::csharp::NativeRarityRegistry ids;
    assert(ids.snapshot()->empty());
    assert(ids.registerType("example", "ExampleModRarity") == 12);
    assert(ids.registerType("example", "ExampleHigherTierModRarity") == 13);
    assert(ids.registerType("second-mod", "ExampleModRarity") == 14);
    assert(ids.findType("example", "ExampleModRarity") == 12);
    assert(ids.findType("second-mod", "ExampleModRarity") == 14);
    assert(ids.findType("missing", "ExampleModRarity") == -1);
    assert(ids.registerType("example", "ExampleModRarity") == -1);
    assert(ids.registerType("bad/mod", "Good") == -1);
    assert(ids.registerType("valid", "Bad-Class") == -1);
    assert(ids.registerType("valid", "") == -1);

    auto old = ids.snapshot();
    assert(old->size() == 3);
    assert(ids.registerType("example", "ThirdRarity") == 15);
    assert(old->size() == 3);
    assert(ids.snapshot()->size() == 4);
    assert((*ids.snapshot())[3].name == "ThirdRarity");
    assert(bl::csharp::sourceContentDomain("BunnyRarity") == "RARITY");
    assert(bl::csharp::sourceIdentityForWire("RARITY", "ThirdRarity") == "RARITY:ThirdRarity");

    const std::string originalExample = R"cs(
        namespace ExampleModCSharp.Rarities;
        public sealed class ExampleModRarity : BunnyRarity {
            public override BunnyColor RarityColor => new(200, 215, 230);
            public override int GetPrefixedRarity(int offset, float valueMult) {
                return offset > 0 ? 13 : Type;
            }
        }
    )cs";
    std::vector<bl::csharp::ParsedSourceType> parsed;
    std::string error;
    assert(bl::csharp::parseCSharpSource(originalExample, &parsed, &error, true));
    assert(parsed.size() == 1);
    assert(parsed[0].base == "BunnyRarity");
    assert(parsed[0].name == "ExampleModRarity");
    assert(parsed[0].namespaceName == "ExampleModCSharp.Rarities");

    const std::string staticDefaults = R"cs(
        public sealed class AnotherRarity : BunnyRarity {
            public override void SetStaticDefaults() { }
            public override BunnyColor RarityColor => new(50, 100, 180);
        }
    )cs";
    parsed.clear();
    assert(bl::csharp::parseCSharpSource(staticDefaults, &parsed, &error, true));
    assert(parsed.size() == 1);
    assert(parsed[0].base == "BunnyRarity");

    std::atomic<bool> reading{true};
    std::atomic<int> observed{0};
    std::thread reader([&] {
        while (reading.load(std::memory_order_relaxed)) {
            const auto snap = ids.snapshot();
            if (!snap->empty()) {
                assert(snap->front().type == 12);
                observed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });
    for (int i = 0; i < 128; ++i)
        assert(ids.registerType("other", "Rarity" + std::to_string(i)) == 16 + i);
    reading.store(false, std::memory_order_relaxed);
    reader.join();
    assert(ids.snapshot()->size() == 132);
    assert(observed.load(std::memory_order_relaxed) >= 0);
}
