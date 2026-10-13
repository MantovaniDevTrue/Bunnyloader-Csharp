#include "experiments/csharp/ManagedVisualState.h"
#include <cassert>
using namespace bl::csharp;
int main() {
    ManagedVisualState state;
    ManagedVisualCommand overlay;
    overlay.kind = ManagedVisualKind::PlayerOverlay;
    overlay.playerIndex = 0;
    overlay.effectId = 123;
    overlay.texture = "ward.png";
    overlay.opacity = 0.4f;
    overlay.durationTicks = 10;
    assert(state.apply(overlay, 100));
    assert(state.size() == 1);
    assert(state.snapshot(0, 8).size() == 1);
    assert(state.snapshot(1, 8).empty());
    assert(state.snapshot(0, 0).empty());
    state.advance(109);
    assert(state.size() == 1);
    overlay.opacity = 0.8f;
    assert(state.apply(overlay, 109));
    assert(state.find(0, 123)->command.opacity == 0.8f);
    state.advance(110);
    assert(state.size() == 1);
    state.advance(119);
    assert(state.size() == 0);
    assert(state.snapshot(0, 8).empty());
    assert(state.apply(overlay, 200));
    ManagedVisualCommand clear;
    clear.kind = ManagedVisualKind::ClearOverlay;
    clear.playerIndex = 0;
    clear.effectId = 123;
    assert(state.apply(clear, 201));
    assert(state.size() == 0);
    assert(state.apply(overlay, 202));
    state.clearPlayer(0);
    assert(state.size() == 0);
    assert(state.apply(overlay, 203));
    state.clear();
    assert(state.size() == 0);
}
