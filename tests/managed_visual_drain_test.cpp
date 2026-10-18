#include "experiments/csharp/ManagedVisualDrain.h"
#include <cassert>
#include <string>
using namespace bl::csharp;

int main() {
    ManagedVisualQueue queue;
    int calls = 0;
    auto next = [&](std::string& out) {
        if (calls == 3) return false;
        out = calls == 1 ? "invalid" :
            "V1|2|0|" + std::to_string(calls) +
            "|0|0|1|0.5|4294967295|5|shield.png";
        ++calls;
        return true;
    };
    auto stats = ManagedVisualDrain::poll(queue, next);
    assert(stats.accepted == 2 && stats.rejected == 1);
    assert(stats.exhausted == 1 && queue.size() == 2);
    queue.clear();
    int produced = 0;
    auto unlimited = [&](std::string& out) {
        out = "V1|1|0|" + std::to_string(produced++) +
            "|0|0|1|1|4294967295|5|spark.png";
        return true;
    };
    auto capped = ManagedVisualDrain::poll(queue, unlimited, 999);
    assert(capped.accepted == 64 && produced == 64);
}
