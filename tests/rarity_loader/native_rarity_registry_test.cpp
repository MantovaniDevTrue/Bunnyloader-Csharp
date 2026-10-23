#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include "experiments/csharp/SourceParser.h"

#include <algorithm>
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

    const auto beforeUnload=ids.snapshot();
    ids.retireMod("example");
    assert(ids.findType("example","ExampleModRarity")==-1);
    assert(!ids.identityForType(12).active);
    assert(ids.identityForType(14).active);
    assert(ids.stableKeyForType(12)=="example/ExampleModRarity");
    assert(beforeUnload->at(0).active); // published snapshots are immutable
    assert(ids.registerType("example","ExampleModRarity")==12);
    assert(ids.registerType("example","ExampleHigherTierModRarity")==13);
    assert(ids.registerType("example","ThirdRarity")==15);
    assert(ids.snapshot()->size()==4);

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

    bl::csharp::NativeRarityRegistry shared;
    assert(shared.reserveExternalType() == 12);
    assert(shared.registerType("csharpMod", "FirstRarity") == 13);
    assert(shared.reserveExternalType() == 14);
    assert(shared.registerType("otherCsharp", "SecondRarity") == 15);
    assert(shared.registerType("csharpMod", "ThirdRarity") == 16);
    const auto sharedSnapshot = shared.snapshot();
    assert(sharedSnapshot->size() == 5);
    assert(!(*sharedSnapshot)[0].managed);
    assert((*sharedSnapshot)[1].managed);
    assert(!(*sharedSnapshot)[2].managed);
    assert((*sharedSnapshot)[3].managed);
    assert((*sharedSnapshot)[4].managed);
    assert(shared.findType("csharpMod", "FirstRarity") == 13);
    assert(shared.identityForType(12).managed == false);
    assert(shared.identityForType(13).managed);
    assert(shared.identityForType(13).name == "FirstRarity");
    assert(shared.identityForType(11).type == -1);
    assert(shared.identityForType(4108).type == -1);
    assert(shared.stableKeyForType(12).empty());
    assert(shared.stableKeyForType(13) == "csharpMod/FirstRarity");
    assert(shared.stableKeyForType(14).empty());
    assert(shared.stableKeyForType(15) == "otherCsharp/SecondRarity");
    assert(shared.typeForStableKey("csharpMod/FirstRarity") == 13);
    assert(shared.typeForStableKey("otherCsharp/SecondRarity") == 15);
    assert(shared.typeForStableKey("csharpMod/MissingRarity") == -1);
    assert(shared.typeForStableKey("csharpMod/FirstRarity/more") == -1);
    assert(shared.typeForStableKey("csharpMod/") == -1);
    assert(shared.typeForStableKey("/FirstRarity") == -1);

    assert(shared.findType("otherCsharp", "SecondRarity") == 15);
    assert(shared.findType("missing", "FirstRarity") == -1);
    assert(shared.reserveExternalType() == 17);
    assert(shared.registerType("otherCsharp", "SecondRarity") == -1);
    bl::csharp::NativeRarityRegistry full;
    for (int id = 12; id < 4108; ++id) assert(full.reserveExternalType() == id);
    assert(full.reserveExternalType() == -1);
    assert(full.registerType("csharp", "Overflow") == -1);
    assert(full.snapshot()->size() == 4096);

    bl::csharp::NativeRarityRegistry simultaneous;
    std::vector<int> external(100), managed(100);
    std::thread jsWriter([&] {
        for (int i = 0; i < 100; ++i)
            external[static_cast<size_t>(i)] = simultaneous.reserveExternalType();
    });
    std::thread csharpWriter([&] {
        for (int i = 0; i < 100; ++i)
            managed[static_cast<size_t>(i)] =
                simultaneous.registerType("concurrentCsharp", "Rarity" + std::to_string(i));
    });
    jsWriter.join();
    csharpWriter.join();
    std::vector<int> unique;
    unique.insert(unique.end(), external.begin(), external.end());
    unique.insert(unique.end(), managed.begin(), managed.end());
    std::sort(unique.begin(), unique.end());
    assert(unique.size() == 200);
    for (size_t i = 0; i < unique.size(); ++i)
        assert(unique[i] == 12 + static_cast<int>(i));
    assert(simultaneous.nextType() == 212);
    assert(simultaneous.snapshot()->size() == 200);
}
