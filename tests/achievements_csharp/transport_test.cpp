#include "experiments/csharp/ManagedAchievementTransport.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

int main() {
    using namespace bl::csharp;
    ManagedAchievementQueue queue;

    AchievementJob invalid;
    invalid.mod = "bad/mod";
    invalid.assembly = "/tmp/mod.dll";
    assert(!queue.push(invalid));
    invalid.mod = "examplemod";
    invalid.operation = 1;
    invalid.value = 12;
    invalid.count = 1;
    invalid.payload = "1";
    assert(queue.push(invalid));
    invalid.payload = "2";
    assert(!queue.push(invalid));
    invalid.payload = "1";
    invalid.count = 0;
    assert(!queue.push(invalid));
    invalid.count = 9999;
    assert(queue.push(invalid));

    AchievementJob out;
    assert(queue.pop(&out) && out.count == 1);
    assert(queue.pop(&out) && out.count == 9999);
    assert(!queue.pop(&out));

    AchievementJob control;
    control.mod = "examplemod";
    control.assembly = "/tmp/mod.dll";
    control.operation = 6;
    control.payload = "BACH1\n";
    control.reply = std::make_shared<AchievementReply>();
    assert(queue.push(control));
    assert(queue.pop(&out));
    assert(out.operation == 6 && out.reply && !out.reply->started);
    assert(!queue.pop(&out));

    control.payload = std::string(ManagedAchievementQueue::MaxPayloadBytes + 1, 'A');
    assert(!queue.push(control));
    control.payload = "BACH1\n";
    control.operation = 5;
    assert(!queue.push(control));
    control.operation = 0;
    control.payload.clear();
    for (std::size_t i = 0; i < ManagedAchievementQueue::Capacity; ++i) {
        control.value = static_cast<int>(i);
        assert(queue.push(control));
    }
    assert(queue.size() == ManagedAchievementQueue::Capacity);
    assert(!queue.push(control));
    for (std::size_t i = 0; i < ManagedAchievementQueue::Capacity; ++i) {
        assert(queue.pop(&out));
        assert(out.value == static_cast<int>(i));
    }
    assert(queue.empty());
    assert(!queue.pop(&out));
    std::cout << "CSHARP_ACHIEVEMENT_MONOVM_QUEUE_PASS=YES\n";
}
