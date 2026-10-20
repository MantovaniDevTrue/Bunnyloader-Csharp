#include "experiments/csharp/ManagedDeferredModRevocation.h"
#include "experiments/csharp/ManagedLiveIngressQueue.h"

#include <atomic>
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp;
    ManagedLiveIngressQueue ingress;
    std::atomic<bool> automatic{true};
    std::atomic<int> primary{850};
    std::atomic<int> revoked{851};
    std::atomic<int> other{852};
    ingress.reset(true);
    const auto admitted = ingress.admission();

    ManagedLiveQueuedEvent first{};
    first.bindingSlot = 0;
    first.tick = 1;
    ManagedLiveQueuedEvent second{};
    second.bindingSlot = 1;
    second.tick = 2;
    ManagedLiveQueuedEvent main{};
    main.bindingSlot = -1;
    main.tick = 3;

    assert(ingress.pushAdditional(admitted, first, 851, revoked).accepted);
    assert(ingress.pushAdditional(admitted, second, 852, other).accepted);
    assert(ingress.push(admitted, main, 850, primary, automatic).accepted);
    assert(ingress.depth() == 3);
    assert(managedLiveCallbackCurrent(true, true, false));
    assert(!managedLiveCallbackCurrent(true, false, false));
    assert(!managedLiveCallbackCurrent(true, true, true));
    assert(!managedLiveCallbackCurrent(false, true, false));

    assert(ingress.pauseBinding(0, revoked) == 1);
    assert(revoked.load() == -1);
    assert(ingress.depth() == 2);
    assert(!ingress.pushAdditional(admitted, first, 851, revoked).accepted);
    assert(!ingress.pushAdditional(ingress.admission(), first, 851, revoked).accepted);
    assert(ingress.pushAdditional(ingress.admission(), second, 852, other).accepted);
    assert(ingress.depth() == 3);

    bool sawPrimary = false;
    int otherEvents = 0;
    ManagedLiveQueuedEvent output{};
    while (ingress.pop(&output)) {
        assert(output.bindingSlot != 0);
        if (output.bindingSlot == -1) sawPrimary = true;
        if (output.bindingSlot == 1) ++otherEvents;
    }
    assert(sawPrimary && otherEvents == 2);

    assert(ingress.push(ingress.admission(), main, 850, primary, automatic).accepted);
    assert(ingress.pushAdditional(ingress.admission(), second, 852, other).accepted);
    assert(ingress.pauseBinding(-1, primary) == 1);
    assert(primary.load() == -1);
    assert(ingress.depth() == 1);
    assert(!ingress.push(admitted, main, 850, primary, automatic).accepted);
    assert(ingress.pop(&output) && output.bindingSlot == 1);
    assert(!ingress.pop(&output));
    std::cout << "MANAGED_LIVE_INGRESS_REVOCATION_OK\n";
}
