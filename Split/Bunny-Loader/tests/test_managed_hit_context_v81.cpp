#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include "experiments/csharp/ManagedHitCallbackDispatch.h"

#include <atomic>
#include <cassert>
#include <type_traits>

using namespace bl::csharp;

int main() {
    static_assert(std::is_trivially_copyable_v<ManagedLiveQueuedEvent>);

    int oldHandler = 1;
    int v2Handler = 2;
    const auto oldOnly = chooseManagedHitCallback(&oldHandler, nullptr, 137);
    assert(oldOnly.method == &oldHandler && !oldOnly.usesV2);
    const auto v2 = chooseManagedHitCallback(&oldHandler, &v2Handler, 137);
    assert(v2.method == &v2Handler && v2.usesV2);
    assert(chooseManagedHitCallback(&oldHandler, &v2Handler, 0).usesV2);
    assert(chooseManagedHitCallback(&oldHandler, &v2Handler, 199).usesV2);
    assert(!chooseManagedHitCallback(&oldHandler, &v2Handler, -1).usesV2);
    assert(!chooseManagedHitCallback(&oldHandler, &v2Handler, 200).usesV2);
    assert(chooseManagedHitCallback(nullptr, &v2Handler, 42).method == &v2Handler);

    const ManagedLiveQueuedEvent legacy{0, 18, false, 0, -1};
    assert(legacy.npcIndex == -1);

    ManagedLiveQueuedEvent hit{0, 44, false, 0, -1, 137};
    ManagedLiveEventQueue fifo;
    ManagedLiveQueuedEvent received;
    assert(fifo.push(legacy));
    assert(fifo.push(hit));
    assert(fifo.pop(&received) && received.npcIndex == -1);
    assert(fifo.pop(&received) && received.npcIndex == 137);

    ManagedLiveIngressQueue ingress;
    std::atomic<int> primary{1137};
    std::atomic<bool> automatic{true};
    const auto ticket = ingress.admission();
    const auto accepted = ingress.push(ticket, hit, 1137, primary, automatic);
    assert(accepted.accepted && !accepted.queueDropped);
    assert(ingress.pop(&received) && received.npcIndex == 137);

    auto older = ingress.admission();
    hit.npcIndex = 138;
    assert(ingress.push(older, hit, 1137, primary, automatic).accepted);
    assert(ingress.pauseBinding(-1, primary) == 1);
    primary.store(1137, std::memory_order_release);
    assert(!ingress.push(older, hit, 1137, primary, automatic).accepted);
    const auto rearmed = ingress.admission();
    assert(ingress.push(rearmed, hit, 1137, primary, automatic).accepted);
    assert(ingress.pop(&received) && received.npcIndex == 138);

    std::atomic<int> secondary{1138};
    hit.bindingSlot = 0;
    hit.npcIndex = 73;
    const auto second = ingress.admission();
    assert(ingress.pushAdditional(second, hit, 1138, secondary).accepted);
    assert(ingress.pop(&received) && received.bindingSlot == 0);
    assert(received.npcIndex == 73);

    hit.isKill = true;
    hit.npcIndex = -1;
    const auto kill = ingress.admission();
    assert(ingress.pushAdditional(kill, hit, 1138, secondary).accepted);
    assert(ingress.pop(&received) && received.isKill && received.npcIndex == -1);
    return 0;
}
