#include "experiments/csharp/SourceParser.h"

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main() {
    const std::string base = "samples/LightsAndShadowsCSharp/Playable/content/";
    const std::array<const char*, 4> names = {
        "main.cs",
        "Common/Lighting/LightingCalculator.cs",
        "Common/Lighting/LightingFrame.cs",
        "Common/Lighting/LightingSettings.cs"
    };
    std::vector<bl::csharp::ParsedSourceType> types;
    for (const char* name : names) {
        std::ifstream file(base + name);
        if (!file) {
            std::cerr << "Missing C# source " << name << '\n';
            return 1;
        }
        const std::string source((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        std::string error;
        if (!bl::csharp::parseCSharpSource(source, &types, &error, false)) {
            std::cerr << name << ": " << error << '\n';
            return 2;
        }
    }
    if (types.size() != 1 || types[0].base != "BunnyScreenEffect" ||
        types[0].name != "LightsAndShadows") return 3;
    std::cout << "LIGHTS_CSHARP_SOURCE_PARSER=PASS files=4" << '\n';
    return 0;
}
