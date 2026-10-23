#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstdio>

int main(int argc, char** argv) {
    assert(argc == 2);
    std::vector<bl::csharp::ParsedSourceType> types;
    std::string error;
    for (const auto& file : std::filesystem::recursive_directory_iterator(argv[1])) {
        if (file.path().extension() != ".cs") continue;
        std::ifstream stream(file.path());
        std::string source((std::istreambuf_iterator<char>(stream)), {});
        if (!bl::csharp::parseCSharpSource(source, &types, &error, false)) {
            std::fprintf(stderr, "%s: %s\n", file.path().c_str(), error.c_str());
            return 1;
        }
    }
    assert(types.size() == 5);
    int native = 0, hook = 0, world = 0;
    for (const auto& type : types) {
        assert(!bl::csharp::sourceIdentityForBase(type.base, type.name).empty());
        native += bl::csharp::sourceRequiresNativeRegistration(type.base);
        hook += type.base == "BunnyWorldUpdateLoader";
        world += type.base == "BunnyWorldSystem";
    }
    assert(native == 2 && hook == 1 && world == 1);
    // A mod containing only an update module must not require an item as a
    // workaround, nor attempt to allocate a Terraria content ID for the module.
    types.clear();
    assert(bl::csharp::parseCSharpSource("public sealed class Alone : BunnyWorldUpdateLoader { }", &types, &error, true));
    assert(types.size() == 1 && !bl::csharp::sourceRequiresNativeRegistration(types[0].base));
    std::puts("FIVE_QA_SOURCE_DISCOVERY_PASS");
}
