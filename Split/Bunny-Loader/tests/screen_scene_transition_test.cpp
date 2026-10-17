#include "graphics/ScreenSceneTransition.h"

#include <cassert>
#include <cmath>

int main() {
    using namespace bl::graphics;
    ScreenSceneTransition transition;
    ScreenSceneSnapshot day;
    day.valid = true;
    day.sunX = 0.45f;
    day.sunY = 0.7f;
    day.strength = 0.8f;
    day.sunVisibility = 1.f;
    day.shadowThreshold = 0.98f;
    day.dayBlend = 1.f;
    auto initial = transition.advance(day, 1.f / 60.f);
    assert(initial.dayBlend == 1.f);
    assert(initial.shadowThreshold == 0.98f);

    ScreenSceneSnapshot night = day;
    night.daytime = false;
    night.dayBlend = 0.f;
    night.shadowThreshold = 0.02f;
    night.strength = 0.45f;
    night.sunVisibility = 0.f;
    night.sunX = -0.3f;
    night.sunY = 1.5f;

    float lastMix = initial.dayBlend;
    float lastLight = initial.strength;
    float lastThreshold = initial.shadowThreshold;
    for (int i = 0; i < 20; ++i) {
        const auto frame = transition.advance(night, 1.f / 60.f);
        assert(frame.valid && !frame.daytime);
        assert(frame.dayBlend >= 0.f && frame.dayBlend < lastMix);
        assert(frame.shadowThreshold >= 0.02f && frame.shadowThreshold < lastThreshold);
        assert(frame.strength < lastLight && frame.strength > 0.45f);
        assert(frame.sunVisibility > 0.f && frame.sunVisibility < 1.f);
        assert(frame.sunX > -0.3f && frame.sunY < 1.5f);
        lastMix = frame.dayBlend;
        lastLight = frame.strength;
        lastThreshold = frame.shadowThreshold;
    }
    const auto unchanged = transition.advance(night, 0.f);
    assert(unchanged.dayBlend == lastMix);
    assert(unchanged.shadowThreshold == lastThreshold);
    transition.reset();
    const auto reset = transition.advance(night, 1.f / 60.f);
    assert(reset.dayBlend == 0.f && reset.shadowThreshold == 0.02f);
    assert(reset.sunVisibility == 0.f && reset.strength == 0.45f);
    return 0;
}
