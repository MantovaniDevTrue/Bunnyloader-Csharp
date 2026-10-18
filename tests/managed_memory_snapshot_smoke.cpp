#include "experiments/csharp/ManagedProcessMemory.h"
#include <cassert>
#include <iostream>

int main() {
    bl::csharp::ManagedProcMemory parsed;
    bl::csharp::parseManagedProcStatusLine(parsed, "Name:\tterraria\\n");
    assert(!parsed.available);
    bl::csharp::parseManagedProcStatusLine(parsed, "VmSize:\t500000 kB");
    bl::csharp::parseManagedProcStatusLine(parsed, "VmHWM:\t160000 kB");
    bl::csharp::parseManagedProcStatusLine(parsed, "VmRSS:\t123456 kB");
    bl::csharp::parseManagedProcStatusLine(parsed, "VmData:\t170000 kB");
    bl::csharp::parseManagedProcStatusLine(parsed, "VmSwap:\t7000 kB");
    bl::csharp::parseManagedProcStatusLine(parsed, "Threads:\t38");
    assert(parsed.available);
    assert(parsed.virtualKb == 500000);
    assert(parsed.peakRssKb == 160000);
    assert(parsed.rssKb == 123456);
    assert(parsed.dataKb == 170000);
    assert(parsed.swapKb == 7000);
    assert(parsed.threads == 38);

    const auto real = bl::csharp::readManagedProcMemory();
    assert(real.available && real.rssKb > 0);
    assert(real.peakRssKb >= real.rssKb);
    assert(real.threads > 0);
    std::cout << "BUNNY_CSHARP_PROC_MEMORY_PARSER=PASS\n";
    std::cout << "BUNNY_CSHARP_ON_DEMAND_MEMORY_SNAPSHOT=PASS\n";
    return 0;
}
