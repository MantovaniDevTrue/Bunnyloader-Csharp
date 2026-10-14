#include "experiments/csharp/ManagedSourceClassCount.h"
#include <cassert>
#include <string>

using bl::csharp::SourceModStatus;
using bl::csharp::expectedManagedSourceTypes;
using bl::csharp::managedSourceCompileFinished;

static std::string finished(const std::string& id, int types) {
    return "state=finished\nmodId=" + id +
        "\nphase=compile-verified\nassemblyReady=1\nexecutedTypes=" +
        std::to_string(types) + "\n";
}

int main() {
    SourceModStatus example;
    example.id = "examplemodcsharp";
    example.items = 4;
    example.projectiles = 2;
    example.buffs = 1;
    example.players = 1;
    example.npcs = 1;
    example.prefixes = 2;
    assert(expectedManagedSourceTypes(example) == 11);
    assert(managedSourceCompileFinished(finished(example.id, 11), example.id, example));
    assert(!managedSourceCompileFinished(finished(example.id, 9), example.id, example));
    assert(!managedSourceCompileFinished(finished("other-mod", 11), example.id, example));
    assert(!managedSourceCompileFinished("state=compiling\n", example.id, example));

    SourceModStatus second;
    second.id = "secondexamplecsharp";
    second.items = 1;
    second.projectiles = 1;
    assert(expectedManagedSourceTypes(second) == 2);
    assert(managedSourceCompileFinished(finished(second.id, 2), second.id, second));
    assert(!managedSourceCompileFinished(finished(second.id, 4), second.id, second));

    SourceModStatus a1;
    a1.id = "a1workbenchprobe";
    a1.items = 5;
    a1.tiles = 1;
    assert(expectedManagedSourceTypes(a1) == 6);
    assert(managedSourceCompileFinished(finished(a1.id, 6), a1.id, a1));
    assert(!managedSourceCompileFinished(finished(a1.id, 5), a1.id, a1));
    assert(!managedSourceCompileFinished(finished(a1.id, 7), a1.id, a1));

    SourceModStatus tilesOnly;
    tilesOnly.id = "tilesonly";
    tilesOnly.tiles = 2;
    assert(expectedManagedSourceTypes(tilesOnly) == 2);
    assert(managedSourceCompileFinished(finished(tilesOnly.id, 2),
                                        tilesOnly.id, tilesOnly));

    SourceModStatus prefixOnly;
    prefixOnly.id = "prefixonly";
    prefixOnly.prefixes = 3;
    assert(expectedManagedSourceTypes(prefixOnly) == 3);
    assert(managedSourceCompileFinished(finished(prefixOnly.id, 3), prefixOnly.id, prefixOnly));
}
