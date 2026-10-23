#include "content/items/ModItemSaveFormat.h"
#include "content/items/ModItemSaveSafety.h"
#include "content/items/NativeRarityPending.h"
#include <chrono>
#include <filesystem>
#include <fstream>
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

    {
        const SavedItem saved{"inventory", 2, 1, 0, false,
            "vanilla:42", "missing/Prefix", {}, "missing/Rarity"};
        auto both = remainingVanillaMetadata(saved, 0, "");
        assert(both.prefixKey == "missing/Prefix");
        assert(both.rarityKey == "missing/Rarity");
        assert(hasRemainingVanillaMetadata(both));
        auto rarityOnly = remainingVanillaMetadata(saved, 5, "");
        assert(rarityOnly.prefixKey.empty());
        assert(rarityOnly.prefix == 0);
        assert(rarityOnly.rarityKey == "missing/Rarity");
        assert(hasRemainingVanillaMetadata(rarityOnly));
        auto prefixOnly = remainingVanillaMetadata(saved, 0, "loaded/Rarity");
        assert(prefixOnly.prefixKey == "missing/Prefix");
        assert(prefixOnly.rarityKey.empty());
        auto resolved = remainingVanillaMetadata(saved, 5, "loaded/Rarity");
        assert(!hasRemainingVanillaMetadata(resolved));
        std::vector<SavedItem> inventory{
            {"inventory", 2, 1, 5, false, "vanilla:42", {}, {}, {}}
        };
        assert(mergePending(inventory, rarityOnly));
        assert(inventory.size() == 1);
        assert(inventory[0].prefix == 5);
        assert(inventory[0].rarityKey == "missing/Rarity");
    }

    {
        const SavedItem rarityOnly{"chest:5,9", 3, 1, 0, false,
            "vanilla:98", {}, {}, "mod/CustomRarity"};
        const SavedItem prefixOnly{"chest:5,9", 3, 1, 0, false,
            "vanilla:98", "mod/CustomPrefix"};
        const SavedItem vanillaPrefix{"chest:5,9", 3, 1, 19, false,
            "vanilla:98"};
        assert(!hasSavedVanillaPrefix(rarityOnly));
        assert(hasRemainingVanillaMetadata(rarityOnly));
        assert(hasSavedVanillaPrefix(prefixOnly));
        assert(hasSavedVanillaPrefix(vanillaPrefix));
        const uint8_t originalPrefix = 22;
        uint8_t rarityOnlyPrefix = originalPrefix;
        if (hasSavedVanillaPrefix(rarityOnly)) rarityOnlyPrefix = 0;
        assert(rarityOnlyPrefix == originalPrefix);
        uint8_t requestedPrefix = originalPrefix;
        if (hasSavedVanillaPrefix(vanillaPrefix))
            requestedPrefix = static_cast<uint8_t>(vanillaPrefix.prefix);
        assert(requestedPrefix == 19);
    }

    {
        struct PendingFixture {
            int type;
            std::string key;
            std::string origin;
        };
        int original = 1, replacement = 2, separate = 3;
        std::map<int*, PendingFixture> refs{
            {&original, {42, "rarity/Original", "inventory#0"}}
        };
        std::map<std::string, PendingFixture> slots{
            {"inventory#0", {42, "rarity/Original", {}}},
            {"inventory#4", {51, "rarity/Other", {}}}
        };
        const auto moved = selectTrackedRarity(refs, slots, &original, "inventory#3", 42);
        assert(moved && moved->key == "rarity/Original");
        assert(moved->origin == "inventory#0");
        const auto changed = selectTrackedRarity(refs, slots, &replacement, "inventory#0", 42);
        assert(!changed);
        const auto other = selectTrackedRarity(refs, slots, &separate, "inventory#4", 51);
        assert(other && other->key == "rarity/Other" &&
            other->origin == "inventory#4");
        assert(!selectTrackedRarity(refs, slots, &original, "inventory#3", 51));
        assert(!selectTrackedRarity(refs, slots, &separate, "inventory#9", 42));
        std::vector<LiveRarityItem<int*>> live{
            {&replacement, "inventory#0", 42, false},
            {&original, "inventory#3", 42, false},
            {&separate, "inventory#4", 51, false}
        };
        std::map<int*, PendingFixture> updatedRefs;
        std::map<std::string, PendingFixture> updatedSlots;
        reconcileTrackedRarities(refs, slots, live, &updatedRefs, &updatedSlots);
        assert(updatedRefs.size() == 2);
        assert(updatedSlots.size() == 2);
        assert(!updatedSlots.contains("inventory#0"));
        assert(updatedSlots.at("inventory#3").key == "rarity/Original");
        assert(updatedRefs.at(&original).origin == "inventory#3");
        assert(updatedSlots.at("inventory#4").key == "rarity/Other");
        const std::vector<LiveRarityItem<int*>> restored{
            {&original, "inventory#3", 42, true},
            {&separate, "inventory#4", 51, false}
        };
        std::map<int*, PendingFixture> onlyUnresolved;
        std::map<std::string, PendingFixture> unresolvedSlots;
        reconcileTrackedRarities(updatedRefs, updatedSlots, restored,
            &onlyUnresolved, &unresolvedSlots);
        assert(onlyUnresolved.size() == 1);
        assert(onlyUnresolved.contains(&separate));
        assert(!unresolvedSlots.contains("inventory#3"));
        assert(unresolvedSlots.contains("inventory#4"));
        const std::vector<LiveRarityItem<int*>> removed;
        reconcileTrackedRarities(updatedRefs, updatedSlots, removed,
            &onlyUnresolved, &unresolvedSlots);
        assert(onlyUnresolved.empty() && unresolvedSlots.empty());
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

    std::vector<SavedItem> protectedRecords{
        {"inventory", 5, 1, 0, false, "stable/Existing", "", "payload", "rarity/Original"}
    };
    std::stringstream damagedV3("bunnyloader itens 3\n"
        "inventory\t0\t1\t0\t0\tmod/Original\t{}\tmod/Rarity\n"
        "inventory\t1\t1\t0\t0\tmod/Broken\t{}\tmod/Bad-Name\n");
    assert(!read(damagedV3, protectedRecords));
    assert(protectedRecords.size() == 1);
    assert(protectedRecords[0].key == "stable/Existing");
    assert(protectedRecords[0].rarityKey == "rarity/Original");

    std::stringstream missingLastColumn("bunnyloader itens 3\n"
        "inventory\t0\t1\t0\t0\tmod/Original\t{}\tmod/Rarity\n"
        "inventory\t1\t1\t0\t0\tmod/NoRarityColumn\t{}\n");
    assert(!read(missingLastColumn, protectedRecords));
    assert(protectedRecords.size() == 1);
    assert(protectedRecords[0].rarityKey == "rarity/Original");

    std::stringstream missingDataColumn("bunnyloader itens 3\n"
        "inventory\t0\t1\t0\t0\tmod/Original\tmod/Rarity\n");
    assert(!read(missingDataColumn, protectedRecords));

    std::stringstream v3MissingNewline("bunnyloader itens 3\n"
        "inventory\t0\t1\t0\t0\tmod/Original\t{}\tmod/Rarity");
    assert(!read(v3MissingNewline, protectedRecords));
    assert(protectedRecords.size() == 1);
    assert(protectedRecords[0].rarityKey == "rarity/Original");
    std::stringstream v3HeaderWithoutNewline("bunnyloader itens 3");
    assert(!read(v3HeaderWithoutNewline, protectedRecords));
    std::stringstream v3HeaderOnly("bunnyloader itens 3\n");
    assert(!read(v3HeaderOnly, protectedRecords));
    assert(protectedRecords.size() == 1);
    std::stringstream v2WithoutNewline("bunnyloader itens 2\n"
        "inventory\t0\t1\t0\t0\tmod/Original\t{}");
    std::vector<SavedItem> legacyWithoutTerminator;
    assert(read(v2WithoutNewline, legacyWithoutTerminator));
    assert(legacyWithoutTerminator.size() == 1);

    std::stringstream truncatedV3("bunnyloader itens 3\n"
        "inventory\t0\t1\t0\t0\tmod/Original\t{}\tmod/Rarity\n"
        "inventory\t1\tone\t0\t0\tmod/Broken\t{}\tmod/Rarity\n");
    assert(!read(truncatedV3, protectedRecords));
    assert(protectedRecords.size() == 1);

    std::stringstream legacyWithExtension("bunnyloader itens 2\ninventory\t0\t1\t0\t1\told/Item\t{}\trmod/Example\n");
    restored.clear();
    assert(read(legacyWithExtension, restored) && restored.empty());
    legacyWithExtension.clear();
    legacyWithExtension.seekg(0);
    const std::vector<SavedItem> savedBeforeStrict = modern;
    restored = savedBeforeStrict;
    assert(!read(legacyWithExtension, restored, true));
    assert(restored.size() == savedBeforeStrict.size());
    assert(restored[0].rarityKey == savedBeforeStrict[0].rarityKey);

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

    const auto directory = std::filesystem::temp_directory_path() /
        ("bunny-rarity-save-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const std::string path = (directory / "player.bl").string();
    assert(canReplaceExisting(path));
    assert(preserveLegacyBeforeRaritySave(path, modern));
    const std::string original = legacy.str();
    {
        std::ofstream output(path, std::ios::binary);
        output << original;
    }
    assert(canReplaceExisting(path));
    assert(preserveLegacyBeforeRaritySave(path, modern));
    const std::string backup = path + ".before-rarity-v3.bak";
    {
        std::ifstream input(backup, std::ios::binary);
        assert(input);
        assert(std::string((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()) == original);
    }
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        assert(write(output, modern));
    }
    assert(canReplaceExisting(path));
    assert(preserveLegacyBeforeRaritySave(path, modern));
    {
        std::ifstream input(backup, std::ios::binary);
        assert(std::string((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()) == original);
    }
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "bunnyloader itens 3\n";
        output << "inventory\t0\t1\t0\t0\tmod/Broken\t{}\tbad/key/more\n";
    }
    assert(!canReplaceExisting(path));
    assert(!preserveLegacyBeforeRaritySave(path, modern));
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "bunnyloader itens 2\n";
        output << "inventory\t0\t1\t0\t0\tmod/Original\t{}\n";
        output << "inventory\t1\t1\t0\t0\tmod/Broken\t{}\tmod/Rarity\n";
    }
    assert(!canReplaceExisting(path));
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << original;
    }
    {
        std::ofstream output(backup, std::ios::binary | std::ios::trunc);
        output << "bunnyloader itens 2\n";
        output << "inventory\t0\t0\t0\t0\tmod/Invalid\t{}\n";
    }
    assert(canReplaceExisting(path));
    assert(!preserveLegacyBeforeRaritySave(path, modern));
    {
        std::ofstream output(backup, std::ios::binary | std::ios::trunc);
        output << original;
    }
    assert(preserveLegacyBeforeRaritySave(path, modern));
    std::filesystem::remove_all(directory);

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
