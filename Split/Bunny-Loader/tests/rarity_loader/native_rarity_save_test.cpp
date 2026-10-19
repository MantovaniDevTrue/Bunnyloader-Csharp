#include "content/items/ModItemSaveFormat.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include <cassert>
#include <sstream>
#include <string>
#include <vector>

using bl::runtime::item_save::SavedItem;

int main() {
    using namespace bl::runtime::item_save;
    std::vector<SavedItem> old{
        {"inventory", 2, 3, 4, false, "mod/OriginalItem", "", R"({"a":123})"},
        {"chest:6,8", 0, 1, 0, true, "mod/OldStaff", "mod/Prefix", "{}"}
    };
    {
        std::vector<SavedItem> active{{"inventory", 4, 1, 0, false, "mod/Sword", "", "{}"}};
        const SavedItem pending{"inventory", 4, 1, 0, false,
            "mod/Sword", "mod/OldPrefix", "ignored", "mod/Rarity"};
        assert(mergePending(active, pending));
        assert(active.size() == 1);
        assert(active[0].prefixKey == "mod/OldPrefix");
        assert(active[0].rarityKey == "mod/Rarity");
        assert(active[0].data == "{}");
        assert(!mergePending(active, {"inventory", 5, 1, 0, false, "mod/Sword"}));
        assert(!mergePending(active, {"inventory", 4, 1, 0, false, "mod/Other"}));
        active[0].prefixKey = "mod/NewPrefix";
        active[0].rarityKey = "mod/Current";
        assert(mergePending(active, pending));
        assert(active[0].prefixKey == "mod/NewPrefix");
        assert(active[0].rarityKey == "mod/Current");
    }

    std::stringstream legacy;
    assert(write(legacy, old));
    assert(legacy.str().starts_with("bunnyloader itens 2\n"));
    std::vector<SavedItem> restored;
    assert(read(legacy, restored));
    assert(restored.size() == 2);
    assert(restored[0].data == R"({"a":123})");
    assert(restored[0].rarityKey.empty());
    assert(restored[1].prefixKey == "mod/Prefix");
    assert(restored[1].rarityKey.empty());

    std::vector<SavedItem> modern = old;
    modern[0].rarityKey = "rmod/ExampleRarity";
    modern[1].rarityKey = "rmod/HigherTier";
    std::stringstream version3;
    assert(write(version3, modern));
    assert(version3.str().starts_with("bunnyloader itens 3\n"));
    assert(version3.str().find(R"({"a":123})") != std::string::npos);
    restored.clear();
    assert(read(version3, restored));
    assert(restored.size() == 2);
    assert(restored[0].rarityKey == "rmod/ExampleRarity");
    assert(restored[1].rarityKey == "rmod/HigherTier");
    assert(restored[1].prefixKey == "mod/Prefix");
    assert(restored[0].data == modern[0].data);

    std::stringstream legacyOne("bunnyloader itens 1\ninventory\t0\t1\t0\t1\told/Item\n");
    restored.clear();
    assert(read(legacyOne, restored) && restored.size() == 1);
    assert(restored[0].rarityKey.empty());

    std::stringstream legacyWithExtension("bunnyloader itens 2\ninventory\t0\t1\t0\t1\told/Item\t{}\trmod/Example\n");
    restored.clear();
    assert(read(legacyWithExtension, restored) && restored.empty());

    std::stringstream mixed("bunnyloader itens 3\ninventory\t0\t1\t0\t1\told/Item\t{}\t\n"
                            "inventory\t1\t1\t0\t0\told/Staff\t{}\trmod/Example\n");
    restored.clear();
    assert(read(mixed, restored) && restored.size() == 2);
    assert(restored[0].rarityKey.empty());
    assert(restored[1].rarityKey == "rmod/Example");

    for (const std::string invalid : {"withoutslash", "/MissingMod", "mod/",
        "mod/a/b", "mod/bad name", "mod/Bad\nClass", "mod/a\tb",
        "mod/Sample/"}) {
        assert(!validRarityKey(invalid));
        auto rejected = modern;
        rejected[0].rarityKey = invalid;
        std::stringstream dest;
        assert(!write(dest, rejected));
    }
    assert(validRarityKey(""));
    assert(validRarityKey("mod.with-dash/Underscore_2"));
    assert(!validRarityKey("mod/Bad-Dash"));

    bl::csharp::NativeRarityRegistry first;
    assert(first.registerType("mod", "One") == 12);
    assert(first.reserveExternalType() == 13);
    assert(first.registerType("mod", "Two") == 14);
    const std::string stable = first.stableKeyForType(14);
    assert(stable == "mod/Two");
    bl::csharp::NativeRarityRegistry reordered;
    assert(reordered.reserveExternalType() == 12);
    assert(reordered.registerType("mod", "Two") == 13);
    assert(reordered.registerType("mod", "One") == 14);
    assert(reordered.typeForStableKey(stable) == 13);
    assert(reordered.typeForStableKey("missing/Two") == -1);
}
