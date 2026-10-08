#include "experiments/csharp/SourceParser.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) return 1;
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
        if (!bl::csharp::parseCSharpSource(source, &definitions, &error)) {
            std::cerr << error << std::endl;
            return 3;
        }
    }
    if (definitions.size() != 2) return 4;
    const auto& bolt = definitions[0];
    const auto& wand = definitions[1];
    if (bolt.name != "SourceBolt" || bolt.base != "BunnyProjectile" ||
        bolt.properties.at("Width") != "10" ||
        bolt.properties.at("Friendly") != "true" ||
        wand.name != "SourceWand" || wand.base != "BunnyWeapon" ||
        wand.properties.at("Shoot") != "SourceBolt" ||
        wand.properties.at("ShootSpeed") != "13f")
        return 5;
    std::vector<bl::csharp::ParsedSourceType> unsupported;
    if (bl::csharp::parseCSharpSource(
        "class Bad : BunnyWeapon { public override void SetDefaults() { Damage = 10 + 2; } }",
        &unsupported, &error)) return 6;
    std::cout << "CSHARP_SOURCE_PARSER_SMOKE_PASS=YES" << std::endl;
    return 0;
}
