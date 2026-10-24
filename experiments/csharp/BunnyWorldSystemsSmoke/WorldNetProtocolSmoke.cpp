#include "experiments/csharp/WorldNetProtocol.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>

int main() {
    using namespace bl::csharp::world_net;
    Snapshot snapshot;
    assert(worldStateDue(1, 0, 0, 1));
    assert(worldStateDue(100, 90, 1, 2));
    assert(!worldStateDue(599, 1, 2, 2));
    assert(worldStateDue(601, 1, 2, 2));
    assert(worldStateDue(600, 0, 2, 2));
    assert(!worldStateDue(101, 102, 2, 2));
    assert(retryIntervalTicks == 600);
    assert(!preferWorldForSendSlot(0));
    assert(preferWorldForSendSlot(1));
    assert(!preferWorldForSendSlot(2));
    assert(preferWorldForSendSlot(3));
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
    assert(!parse("WS1|9223372036854775808|mod|World|1|0101", &snapshot));
    assert(!parse("WS1|42|mod|World|9223372036854775808|0101", &snapshot));
    assert(parse("WS1|9223372036854775807|mod|World|9223372036854775807|0101", &snapshot));
    assert(!parse("WS1|42|mod|World|18446744073709551616|0101", &snapshot));
    assert(!parse(std::string("WS1|42|mod|World|1|") + std::string(194, '0'), &snapshot));
    assert(!parse("INV1|42|mod|World|1|0101", &snapshot));
    assert(parse("WS1|999|mod|World|2|01", &snapshot));
    assert(snapshot.session == 999 && snapshot.revision == 2);
    std::deque<Queued> queue;
    assert(parse("WS1|42|mod-a|Progress|1|0101", &snapshot));
    assert(enqueueLatest(&queue, 5, snapshot, "rev1", 2));
    assert(queue.size() == 1 && queue.front().payload == "rev1");
    assert(!enqueueLatest(&queue, 5, snapshot, "replay", 2));
    assert(queue.size() == 1 && queue.front().payload == "rev1");
    assert(parse("WS1|42|mod-a|Progress|3|0101", &snapshot));
    assert(enqueueLatest(&queue, 5, snapshot, "rev3", 2));
    assert(queue.size() == 1 && queue.front().revision == 3 &&
           queue.front().payload == "rev3");
    assert(parse("WS1|42|mod-a|Progress|2|0101", &snapshot));
    assert(!enqueueLatest(&queue, 5, snapshot, "older", 2));
    assert(queue.front().payload == "rev3");
    assert(parse("WS1|43|mod-a|Progress|4|0101", &snapshot));
    assert(!enqueueLatest(&queue, 5, snapshot, "wrong-host-world", 2));
    assert(parse("WS1|42|mod-b|Progress|1|0101", &snapshot));
    assert(enqueueLatest(&queue, 5, snapshot, "different-mod", 2));
    assert(queue.size() == 2);
    assert(parse("WS1|42|mod-c|Progress|1|0101", &snapshot));
    assert(!enqueueLatest(&queue, 5, snapshot, "full", 2));
    assert(parse("WS1|42|mod-a|Progress|4|0101", &snapshot));
    assert(enqueueLatest(&queue, 5, snapshot, "rev4-when-full", 2));
    assert(queue.front().payload == "rev4-when-full" && queue.size() == 2);
    queue.clear();
    assert(enqueueLatest(&queue, 6, snapshot, "new-local-world", 2));
    assert(queue.front().localSession == 6 && queue.front().revision == 4);
    std::map<int, int> clients = {{2, 0}, {5, 0}, {11, 0}};
    assert(nextPeerAfter(clients, -1)->first == 2);
    assert(nextPeerAfter(clients, 2)->first == 5);
    assert(nextPeerAfter(clients, 5)->first == 11);
    assert(nextPeerAfter(clients, 11)->first == 2);
    clients.erase(5);
    assert(nextPeerAfter(clients, 2)->first == 11);
    assert(nextPeerAfter(clients, 10)->first == 11);
    clients.erase(11);
    assert(nextPeerAfter(clients, 2)->first == 2);
    clients.clear();
    assert(nextPeerAfter(clients, 2) == clients.end());
    std::cout << "Bunny Engine world network protocol: PASS\n";
    return 0;
}
