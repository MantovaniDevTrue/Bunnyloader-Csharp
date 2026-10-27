#include "experiments/csharp/NativeWorldTickOutcome.h"
#include "experiments/csharp/NativeRarityRetryPolicy.h"
#include <cassert>

int main() {
    using namespace bl::csharp;
    static_assert(worldTickUnsupported(-2));
    static_assert(!worldTickFailed(-2));
    static_assert(worldTickFailed(-1));
    static_assert(!worldTickFailed(0));
    static_assert(!worldTickFailed(4));
    static_assert(!rarityRetryPending("READY|FF112233"));
    static_assert(rarityRetryPending("PENDING|Stage=SubmitRarityCallback Reason=WorkerQueueBusy"));
    static_assert(rarityRetryPending("PENDING|Stage=WaitRarityCallback Reason=DeadlineExceeded"));
    static_assert(!rarityRetryPending("ERROR|Stage=Decode"));
    static_assert(raritySampleReady(100,0));
    static_assert(!raritySampleReady(101,rarityRetryAt(100)));
    static_assert(!raritySampleReady(129,rarityRetryAt(100)));
    static_assert(raritySampleReady(130,rarityRetryAt(100)));
    return 0;
}
