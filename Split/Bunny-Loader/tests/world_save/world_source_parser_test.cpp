#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

int main() {
    const std::string worldOnly =
        "using BunnyLoader.Source;\n"
        "namespace ExampleModCSharp;\n"
        "public sealed class DownedBossSystem : BunnyWorldSystem {\n"
        " public override void ClearWorld() { }\n"
        " public override void SaveWorldData(BunnyWorldTag tag) { }\n"
        "}\n";
    std::vector<bl::csharp::ParsedSourceType> defs;
    std::string error;
    assert(bl::csharp::parseCSharpSource(worldOnly, &defs, &error, true));
    assert(defs.size() == 1 && defs[0].base == "BunnyWorldSystem");
    assert(defs[0].name == "DownedBossSystem");
    assert(defs[0].namespaceName == "ExampleModCSharp");
    assert(defs[0].properties.empty());

    defs.clear();
    const std::string mixed = worldOnly +
        "public sealed class SecondSystem : BunnyWorldSystem { }\n";
    assert(bl::csharp::parseCSharpSource(mixed, &defs, &error, true));
    assert(defs.size() == 2);
    defs.clear();
    const std::string abstractOnly =
        "public abstract class BaseWorld : BunnyWorldSystem { }";
    assert(!bl::csharp::parseCSharpSource(abstractOnly, &defs, &error, true));
    std::puts("WORLD_ONLY_SOURCE_PARSER_PASS");
}
