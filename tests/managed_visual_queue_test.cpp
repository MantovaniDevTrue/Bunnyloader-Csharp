#include "experiments/csharp/ManagedVisualQueue.h"
#include <cassert>

using namespace bl::csharp;

int main() {
    ManagedVisualQueue queue;
    for (int i = 0; i < 64; ++i) {
        ManagedVisualCommand command;
        command.playerIndex = 0;
        command.effectId = i;
        command.kind = ManagedVisualKind::Particle;
        command.durationTicks = 2;
        assert(queue.push(command));
    }
    ManagedVisualCommand extra;
    extra.playerIndex = 0;
    extra.effectId = 65;
    assert(!queue.push(extra));
    for (int i = 0; i < 64; ++i) {
        ManagedVisualCommand command;
        assert(queue.pop(command));
        assert(command.effectId == i);
    }
    assert(queue.size() == 0);
    ManagedVisualCommand overlay;
    overlay.kind = ManagedVisualKind::PlayerOverlay;
    overlay.playerIndex = 0;
    overlay.effectId = 8;
    overlay.texture = "shield.png";
    overlay.opacity = 0.3f;
    assert(queue.push(overlay));
    overlay.opacity = 0.8f;
    assert(queue.push(overlay));
    assert(queue.size() == 1);
    assert(queue.pop(extra));
    assert(extra.opacity == 0.8f);
    assert(queue.push(overlay));
    ManagedVisualCommand clear;
    clear.kind = ManagedVisualKind::ClearOverlay;
    clear.playerIndex = 0;
    clear.effectId = 8;
    assert(queue.push(clear));
    assert(queue.size() == 1);
    assert(queue.pop(extra));
    assert(extra.kind == ManagedVisualKind::ClearOverlay);
    queue.clear();
    assert(queue.size() == 0);
}
