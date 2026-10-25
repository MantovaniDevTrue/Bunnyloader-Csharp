#include "experiments/csharp/SourceLoader.cpp"
#include "experiments/csharp/ContentTexture.h"
#include <cassert>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    using namespace bl::csharp;
    assert(argc == 2);
    const std::filesystem::path root(argv[1]);
    std::vector<ParsedSourceType> types;
    std::string error;
    size_t files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.path().extension() != ".cs") continue;
        std::ifstream input(entry.path());
        assert(input.good());
        const std::string source((std::istreambuf_iterator<char>(input)), {});
        if (!parseCSharpSource(source, &types, &error, false)) {
            std::cerr << entry.path() << ": " << error << '\n';
            return 1;
        }
        ++files;
    }
    assert(files >= 97);
    assert(std::any_of(types.begin(), types.end(), [](const auto& type) {
        return type.name == "ExampleHigherTierModRarity" && type.base == "BunnyRarity";
    }));
    std::ifstream registrationSource("app/src/main/cpp/experiments/csharp/SourceLoader.cpp");
    assert(registrationSource.good());
    const std::string registrationText((std::istreambuf_iterator<char>(registrationSource)), {});
    const std::string rarityMarker = "++status->rarities;";
    const size_t rarityOffset = registrationText.find(rarityMarker);
    assert(rarityOffset != std::string::npos);
    const size_t nextStatement = registrationText.find_first_not_of(" \t\r\n",
        rarityOffset + rarityMarker.size());
    assert(nextStatement != std::string::npos);
    assert(registrationText.compare(nextStatement, 9, "continue;") == 0);
    size_t checked = 0;
    size_t commands = 0;
    std::map<std::string, std::string> targets;
    for (const auto& type : types) {
        if (type.base == "BunnyCommand") {
            ++commands;
            continue;
        }
        if (!sourceRequiresNativeRegistration(type.base) ||
            type.base == "BunnyScreenEffect" ||
            type.base == "BunnyRarity" || type.base == "BunnyPrefix" ||
            type.base == "BunnyDust") continue;
        NativeContentSpec spec{};
        std::string target, display, texture;
        if (!fields(type, &spec, &target, &display, &texture, &error)) {
            std::cerr << type.name << ": " << error << '\n';
            return 1;
        }
        std::vector<std::string> requiredTextures;
        if (type.base == "BunnyMount") {
            const auto kind = type.properties.find("Kind");
            if (kind == type.properties.end() ||
                (kind->second != "Ride" && kind->second != "Minecart")) {
                std::cerr << type.name << ": invalid mount kind\n";
                return 1;
            }
            requiredTextures.push_back(texture + "_Front.png");
            if (kind->second == "Ride") {
                requiredTextures.push_back(texture + "_Back.png");
                const auto balloon = type.properties.find("BalloonTexture");
                if (balloon == type.properties.end() || balloon->second.empty()) {
                    std::cerr << type.name << ": balloon texture missing from mount definition\n";
                    return 1;
                }
                requiredTextures.push_back(balloon->second);
            }
        } else {
            requiredTextures.push_back(texture);
        }
        for (const auto& required : requiredTextures) {
            std::string normalized;
            if (!normalizeContentTexture(required.c_str(), &normalized, &error) ||
                !std::filesystem::is_regular_file(root / normalized)) {
                std::cerr << type.name << ": missing or invalid asset "
                          << required << " (" << error << ")\n";
                return 1;
            }
        }
        assert(!display.empty());
        if (!target.empty()) {
            const std::string base = type.base == "BunnyBuffPotion" ? "BunnyBuff" : "BunnyProjectile";
            assert(std::any_of(types.begin(), types.end(), [&](const auto& other) {
                return other.base == base && other.name == target;
            }));
            targets.emplace(type.name, target);
        }
        ++checked;
    }
    assert(commands == 4);
    assert(targets.at("ExampleBuffPotion") == "ExampleDefenseBuff");
    assert(targets.at("ExampleWand") == "ExampleBolt");
    // Calls on either side of native references must remain deferred to Mono.
    types.clear();
    assert(parseCSharpSource(R"cs(public sealed class IndependentPotion : BunnyBuffPotion {
        public override void SetDefaults() {
            base.SetDefaults(); DisplayName="Independent Potion"; Texture="independent.png";
            SetRarity<MyRarity>(); Buff=nameof(MyBuff); Helpers.Configure(1, Math.Max(2,3)); Duration=600;
        }
    })cs", &types, &error));
    NativeContentSpec spec{};
    std::string target, display, texture;
    assert(fields(types.at(0), &spec, &target, &display, &texture, &error));
    assert(target == "MyBuff" && spec.timeLeft == 600);
    assert(!types[0].properties.contains("SetRarity"));
    std::cout << "FULL_EXAMPLE_NATIVE_BOOTSTRAP_PASS files=" << files << " registrations=" << checked
              << " assets=Present dependencies=Resolved managedCalls=DeferredToMono\n";
}
