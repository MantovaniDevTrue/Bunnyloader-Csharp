#include "graphics/FrameComposer.h"

#include <cassert>
#include <cmath>
#include <string>
#include <utility>

using namespace bl::graphics;

static EffectFrame sample(int id) {
    EffectFrame frame;
    frame.targets.push_back({id, "bloom", 0.333333f, false});
    EffectDraw threshold;
    threshold.shader = 1;
    threshold.dest = id;
    threshold.source = kTexScreen;
    frame.draws.push_back(threshold);
    EffectDraw composite;
    composite.shader = 2;
    composite.source = id;
    composite.textures.push_back({"uBloom", id});
    frame.draws.push_back(composite);
    return frame;
}

int main() {
    EffectFrame result;
    std::string error;
    assert(appendNamespacedFrame(result, sample(51001), "lights", &error));
    const int first = result.targets.front().id;
    assert(result.targets.front().name == "lights:bloom");
    assert(result.draws[0].dest == first);
    assert(result.draws[1].source == first);
    assert(result.draws[1].textures[0].texture == first);

    assert(appendNamespacedFrame(result, sample(51001), "other", &error));
    const int second = result.targets.back().id;
    assert(second != first);
    assert(result.targets.back().name == "other:bloom");
    assert(result.draws.back().textures[0].texture == second);

    const size_t prior = result.draws.size();
    auto broken = sample(1);
    broken.draws[0].dest = 42;
    assert(!appendNamespacedFrame(result, std::move(broken), "broken", &error));
    assert(error == "unresolved render target reference");
    assert(result.draws.size() == prior);

    auto badValue = sample(1);
    badValue.draws[0].intensity = NAN;
    assert(!appendNamespacedFrame(result, std::move(badValue), "broken", &error));
    assert(result.draws.size() == prior);
    assert(!appendNamespacedFrame(result, sample(1), "../escape", &error));

    EffectFrame history;
    history.history = true;
    assert(appendNamespacedFrame(result, std::move(history), "camera"));
    assert(result.history);

    auto duplicate = sample(1);
    duplicate.targets.push_back({1, "also-bloom", 1.f, false});
    assert(!appendNamespacedFrame(result, std::move(duplicate), "invalid", &error));
    return 0;
}
