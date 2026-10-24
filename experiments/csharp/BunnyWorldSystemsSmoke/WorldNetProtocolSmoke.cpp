#include "experiments/csharp/WorldNetProtocol.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using namespace bl::csharp::world_net;
    Snapshot snapshot;
    assert(parse("WS1|42|mod-a|DownedBossSystem|1|0101", &snapshot));
    assert(snapshot.session == 42 && snapshot.revision == 1 &&
           snapshot.mod == "mod-a" && snapshot.system == "DownedBossSystem");
    assert(!parse("WS1|0|mod|World|1|0101", &snapshot));
    assert(!parse("WS1|42|mod|World|0|0101", &snapshot));
    assert(!parse("WS1|42|mod|World|1|010", &snapshot));
    assert(!parse("WS1|42|mod|World|1|0G", &snapshot));
    assert(!parse("WS1|42|mod|World|1|0a", &snapshot));
    assert(!parse("WS1|42|../mod|World|1|0101", &snapshot));
    assert(!parse("WS1|42|mod|../World|1|0101", &snapshot));
    assert(!parse("WS1|42|mod|World|1|0101|extra", &snapshot));
    assert(!parse("WS1|42|mod|World|18446744073709551616|0101", &snapshot));
    assert(!parse(std::string("WS1|42|mod|World|1|") + std::string(194, '0'), &snapshot));
    assert(!parse("INV1|42|mod|World|1|0101", &snapshot));
    assert(parse("WS1|999|mod|World|2|01", &snapshot));
    assert(snapshot.session == 999 && snapshot.revision == 2);
    std::cout << "Bunny Engine world network protocol: PASS\n";
    return 0;
}
