#include "experiments/csharp/SourceParser.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 7) return 1;
    std::vector<bl::csharp::ParsedSourceType> definitions;
    std::string error;
    for (int i = 1; i < argc; ++i) {
        const std::string path = argv[i];
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) return 2;
        std::string source;
        char buffer[4096];
        for (size_t n; (n = std::fread(buffer, 1, sizeof(buffer), file)) > 0;)
            source.append(buffer, n);
        std::fclose(file);
        if (!bl::csharp::parseCSharpSource(source, &definitions, &error, false)) {
            std::cerr << error << std::endl;
            return 3;
        }
    }
    std::vector<bl::csharp::ParsedSourceType> helperOnly;
    std::string helper = "namespace Example; public static class Motion { public static float Curve(int t) { return MathF.Sin(t * 0.22f) * 0.03f; } }";
    if (bl::csharp::parseCSharpSource(helper, &helperOnly, &error)) return 7;
    if (!helperOnly.empty()) return 8;
    error.clear();
    if (!bl::csharp::parseCSharpSource(helper, &helperOnly, &error, false)) return 9;
    if (!helperOnly.empty()) return 10;
    if (definitions.size() != 4) return 4;
    const auto& bolt = definitions[0];
    const auto& wand = definitions[1];
    const auto& spark = definitions[2];
    const auto& staff = definitions[3];
    if (bolt.name != "SourceBolt" || bolt.base != "BunnyProjectile" ||
        bolt.properties.count("Width") != 0 ||
        bolt.properties.count("Friendly") != 0 ||
        wand.name != "SourceWand" || wand.base != "BunnyWeapon" ||
        wand.properties.at("Shoot") != "SourceBolt" ||
        wand.properties.at("ShootSpeed") != "13f" ||
        spark.name != "SourceSpark" || spark.base != "BunnyProjectile" ||
        spark.properties.count("Width") != 0 ||
        spark.properties.at("Texture") != "Content/Projectiles/SourceBolt.png" ||
        staff.name != "SourceStaff" || staff.base != "BunnyWeapon" ||
        staff.properties.at("Shoot") != "SourceSpark" ||
        staff.properties.at("Texture") != "Content/Items/Weapons/SourceWand.png")
        return 5;
    std::vector<bl::csharp::ParsedSourceType> unsupported;
    if (bl::csharp::parseCSharpSource(
        "class Bad : BunnyWeapon { public override void SetDefaults() { Damage = 10 + 2; } }",
        &unsupported, &error)) return 6;
    std::vector<bl::csharp::ParsedSourceType> statefulFields;
    error.clear();
    if (!bl::csharp::parseCSharpSource(
        "class Stateful : BunnyProjectile { public int HitsTaken { get; private set; }"
        " public int LastDamageTaken { get; private set; }"
        " private int score = 1;"
        " public override void OnHitNPC(int damage) { score = damage; }"
        " public override void SetDefaults() { Width = 10; }"
        " public int Total() => score + HitsTaken; }",
        &statefulFields, &error)) {
        std::cerr << "Stateful C# member rejected: " << error << std::endl;
        return 11;
    }
    if (statefulFields.size() != 1 ||
        !statefulFields[0].properties.empty())
        return 18;
    std::vector<bl::csharp::ParsedSourceType> methods;
    error.clear();
    if (!bl::csharp::parseCSharpSource(
        "class Safe : BunnyProjectile { public override void SetDefaults() { Width = 10; } public int OnHitBuffId() => 24; public override int OnHitNPC(int dmg) { return dmg + 1; } }",
        &methods, &error)) return 12;
    if (methods.size() != 1 || !methods[0].properties.empty())
        return 13;
    std::vector<bl::csharp::ParsedSourceType> invalidField;
    error.clear();
    if (bl::csharp::parseCSharpSource(
        "class Incomplete : BunnyWeapon { public int HitsTaken { get;"
        " public override void SetDefaults() { Damage = 10; } }",
        &invalidField, &error)) return 19;
    std::vector<bl::csharp::ParsedSourceType> doubleDefaults;
    error.clear();
    if (bl::csharp::parseCSharpSource(
        "class Duplicate : BunnyWeapon { public override void SetDefaults() { Damage = 10; }"
        " public override void SetDefaults() { Damage = 20; } }",
        &doubleDefaults, &error)) return 20;
    std::ifstream liveInput(argv[6], std::ios::binary);
    if (!liveInput) return 14;
    const std::string liveSource(
        (std::istreambuf_iterator<char>(liveInput)),
        std::istreambuf_iterator<char>());
    if (liveSource.find("||") == std::string::npos ||
        liveSource.find("&&") == std::string::npos ||
        liveSource.find('?') == std::string::npos)
        return 15;
    std::vector<bl::csharp::ParsedSourceType> liveDefinitions;
    error.clear();
    if (!bl::csharp::parseCSharpSource(liveSource, &liveDefinitions, &error, false)) {
        std::cerr << "Live NPC C# rejected: " << error << std::endl;
        return 16;
    }
    if (!liveDefinitions.empty()) return 17;
    std::cout << "CSHARP_LIVE_NPC_SOURCE_PARSER_PASS=YES" << std::endl;
    std::cout << "CSHARP_SOURCE_PARSER_SMOKE_PASS=YES" << std::endl;
    return 0;
}
