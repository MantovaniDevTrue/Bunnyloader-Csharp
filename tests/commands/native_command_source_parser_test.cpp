#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include "experiments/csharp/ManagedSourceClassCount.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    const std::string example = R"(namespace Commands;
public sealed class Heal : BunnyCommand
{
    public override string Command => "heal";
    public override void Action(BunnyCommandContext ctx)
    {
        ctx.Caller.Reply("ok");
    }
}
public sealed class Echo : BunnyCommand
{
    public override string Command => "echo";
    public override string Description => "texto";
    public override void Action(BunnyCommandContext context) { }
}
)";
    std::vector<ParsedSourceType> definitions;
    std::string error;
    assert(parseCSharpSource(example, &definitions, &error, true));
    assert(definitions.size() == 2);
    assert(definitions[0].base == "BunnyCommand");
    assert(definitions[0].name == "Heal");
    assert(definitions[0].namespaceName == "Commands");
    assert(sourceIdentityForBase("BunnyCommand", "Heal") == "COMMAND:Heal");
    assert(!sourceRequiresNativeRegistration("BunnyCommand"));
    SourceModStatus commandsOnly;
    commandsOnly.commands = 2;
    assert(expectedManagedSourceTypes(commandsOnly) == 2);
    assert(managedSourceCompileFinished(
        "state=finished\nmodId=example\nphase=compile-verified\nassemblyReady=1\nexecutedTypes=2\n",
        "example", commandsOnly));
    assert(!managedSourceCompileFinished(
        "state=finished\nmodId=example\nphase=compile-verified\nassemblyReady=1\nexecutedTypes=1\n",
        "example", commandsOnly));
    definitions.clear();
    error.clear();
    assert(parseCSharpSource("namespace Example; public class Sword : BunnyItem { public override void SetDefaults() { DisplayName = \"Test\"; Texture = \"test.png\"; } }",
        &definitions, &error, true));
    assert(definitions.size() == 1 && definitions[0].base == "BunnyItem");
    std::cout << "BUNNY_CSHARP_COMMAND_PARSER=PASS" << std::endl;
}
