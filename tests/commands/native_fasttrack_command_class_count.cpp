#include "experiments/csharp/ManagedSourceClassCount.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    bl::csharp::SourceModStatus mod;
    mod.items = 2;
    mod.dusts = 1;
    mod.rarities = 1;
    mod.localizationModules = 1;
    mod.commands = 3;
    assert(bl::csharp::expectedManagedSourceTypes(mod) == 8);

    const std::string status =
        "state=finished\n"
        "modId=example\n"
        "phase=compile-verified\n"
        "assemblyReady=1\n"
        "executedTypes=8\n";
    assert(bl::csharp::managedSourceCompileFinished(status, "example", mod));
    assert(!bl::csharp::managedSourceCompileFinished(status, "other", mod));

    mod.commands = 2;
    assert(bl::csharp::expectedManagedSourceTypes(mod) == 7);
    assert(!bl::csharp::managedSourceCompileFinished(status, "example", mod));

    mod.commands = 0;
    mod.items = 0;
    mod.dusts = 0;
    mod.rarities = 0;
    mod.localizationModules = 0;
    assert(bl::csharp::expectedManagedSourceTypes(mod) == 0);
    std::cout << "CSHARP_FASTTRACK_COMMAND_TYPE_COUNT=PASS\n";
}
