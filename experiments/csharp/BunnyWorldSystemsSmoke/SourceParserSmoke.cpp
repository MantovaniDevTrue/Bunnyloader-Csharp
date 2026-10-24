#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::string source = R"(
namespace BunnyWorldSystemsSmoke;
public sealed class Progress : BunnyWorldSystem
{
    public override void OnWorldEnter(in BunnyWorldContext world) { }
    public override void OnWorldTick(in BunnyWorldContext world) { }
    public override void OnWorldExit() { }
}
)";
    std::vector<bl::csharp::ParsedSourceType> types;
    std::string error;
    bool parsed = bl::csharp::parseCSharpSource(source, &types, &error);
    assert(parsed && error.empty());
    assert(types.size() == 1);
    assert(types[0].base == "BunnyWorldSystem");
    assert(types[0].name == "Progress");
    assert(bl::csharp::sourceIdentityForBase(types[0].base, types[0].name) == "SYSTEM:Progress");
    assert(!bl::csharp::sourceRequiresNativeRegistration(types[0].base));
    std::cout << "Bunny world-system native parser: PASS\n";
    return 0;
}
