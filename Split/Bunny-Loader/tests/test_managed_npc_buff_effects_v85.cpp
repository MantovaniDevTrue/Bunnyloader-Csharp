#include "experiments/csharp/ManagedNpcBuffEffects.h"
#include <cassert>
#include <cstdio>
using namespace bl::csharp;
int main() {
    int id = 0;
    int duration = 0;
    assert(decodeManagedNpcBuffCode(1024180, &id, &duration));
    assert(id == 24 && duration == 180);
    assert(decodeManagedNpcBuffCode(1044120, &id, &duration));
    assert(id == 44 && duration == 120);
    assert(decodeManagedNpcBuffCode(1039240, &id, &duration));
    assert(id == 39 && duration == 240);
    assert(!decodeManagedNpcBuffCode(1044000, &id, &duration));
    assert(!decodeManagedNpcBuffCode(1044700, &id, &duration));
    assert(!decodeManagedNpcBuffCode(1050000, &id, &duration));
    assert(!decodeManagedNpcBuffCode(30000, &id, &duration));
    int dir = 0;
    int power = 0;
    assert(decodeManagedNpcImpulseCode(2000135, &dir, &power));
    assert(dir == 1 && power == 35);
    assert(decodeManagedNpcImpulseCode(2000260, &dir, &power));
    assert(dir == 2 && power == 60);
    assert(!decodeManagedNpcImpulseCode(2000009, &dir, &power));
    assert(!decodeManagedNpcImpulseCode(2000300, &dir, &power));
    assert(!decodeManagedNpcImpulseCode(2000261, &dir, &power));
    assert(!decodeManagedNpcImpulseCode(2000235, nullptr, &power));
    assert(!decodeManagedNpcImpulseCode(1000235, &dir, &power));
    ManagedNpcBuffEffectQueue queue;
    ManagedNpcBuffCommand input;
    input.npcIndex = 36;
    input.npcType = 320;
    input.buffId = 44;
    input.durationTicks = 180;
    input.generation = 3;
    for (unsigned i = 0; i < ManagedNpcBuffEffectQueue::kCapacity; ++i)
        assert(queue.push(input));
    assert(queue.depth() == 64);
    assert(!queue.push(input));
    ManagedNpcBuffCommand output;
    for (unsigned i = 0; i < ManagedNpcBuffEffectQueue::kCapacity; ++i) {
        assert(queue.peek(&output));
        assert(output.npcIndex == 36 && output.buffId == 44);
        queue.pop();
    }
    assert(queue.depth() == 0 && !queue.peek(&output));
    assert(queue.push(input));
    assert(queue.peek(&output) && output.durationTicks == 180);
    queue.pop();
    std::puts("CSHARP_V85_NPC_EFFECT_CODEC_AND_FIFO=PASS");
}
