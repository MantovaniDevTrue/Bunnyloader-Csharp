#include "content/items/ModItemSaveFormat.h"
#include "content/items/ModItemSaveSafety.h"
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
