#include "experiments/csharp/SourceParser.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) return 1;
    const char* expectedNames[] = {"ExampleGuardianShieldAccessory",
        "ExampleTrailblazerBootsAccessory", "ExampleMysticWardAccessory"};
    const char* expectedModes[] = {"EquippedSprite", "RunningTrail", "EnergyBarrier"};
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        if (!file) return 2;
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        std::vector<bl::csharp::ParsedSourceType> definitions;
        std::string error;
        if (!bl::csharp::parseCSharpSource(source, &definitions, &error, true)) {
            std::cerr << argv[i] << ": " << error << std::endl;
            return 3;
        }
        if (definitions.size() != 1 ||
            definitions[0].base != "BunnyAccessory" ||
            definitions[0].name != expectedNames[i - 1] ||
            definitions[0].properties.at("VisualMode") != expectedModes[i - 1] ||
            definitions[0].properties.at("VisualColor").rfind("0x", 0) != 0 ||
            definitions[0].properties.at("VisualScale").empty() ||
            definitions[0].properties.at("VisualOpacity").empty() ||
            definitions[0].properties.at("VisualTexture").empty())
            return 4;
    }
    std::cout << "CSHARP_ACCESSORY_VISUAL_SOURCE_PARSER_PASS=YES" << std::endl;
    return 0;
}
