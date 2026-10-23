#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) return 2;
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    std::vector<bl::csharp::ParsedSourceType> types;
    std::string error;
    if (!bl::csharp::parseCSharpSource(source, &types, &error, true))
        return 3;
    assert(types.size() == 1);
    assert(types[0].name == "ManyWormsKilled");
    assert(types[0].base == "BunnyAchievement");
    assert(types[0].namespaceName == "ExampleMod.Content.Achievements");
    assert(bl::csharp::sourceContentDomain("BunnyAchievement") == "ACHIEVEMENT");
    assert(bl::csharp::sourceIdentityForBase("BunnyAchievement", "ManyWormsKilled") ==
           "ACHIEVEMENT:ManyWormsKilled");
    assert(!bl::csharp::sourceRequiresNativeRegistration("BunnyAchievement"));
    assert(bl::csharp::sourceRequiresNativeRegistration("BunnyNPC"));
    std::vector<bl::csharp::ParsedSourceType> empty;
    assert(!bl::csharp::parseCSharpSource(
        "public sealed class OrdinaryHelper { }", &empty, &error, true));
    assert(empty.empty());
    return 0;
}
