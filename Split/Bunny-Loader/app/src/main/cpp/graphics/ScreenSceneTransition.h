#pragma once

#include "graphics/ScreenScene.h"

#include <algorithm>
#include <cmath>

namespace bl::graphics {

class ScreenSceneTransition {
public:
    void reset() {
        initialized_ = false;
        current_ = ScreenSceneSnapshot{};
    }

    ScreenSceneSnapshot advance(const ScreenSceneSnapshot& input, float seconds) {
        if (!input.valid) return initialized_ ? current_ : input;
        if (!initialized_) {
            current_ = input;
            initialized_ = true;
            return current_;
        }
        const float dt = std::clamp(seconds, 0.0f, 0.1f);
        const float positionRate = 1.0f - std::exp(-dt / 0.085f);
        const float lightingRate = 1.0f - std::exp(-dt / 0.22f);
        const float transitionRate = 1.0f - std::exp(-dt / 0.36f);
        current_.sunX += (input.sunX - current_.sunX) * positionRate;
        current_.sunY += (input.sunY - current_.sunY) * positionRate;
        current_.clock = input.clock;
        current_.strength += (input.strength - current_.strength) * lightingRate;
        current_.shadowThreshold += (input.shadowThreshold - current_.shadowThreshold) * transitionRate;
        current_.dayBlend += (input.dayBlend - current_.dayBlend) * transitionRate;
        current_.sunVisibility += (input.sunVisibility - current_.sunVisibility) * lightingRate;
        current_.valid = true;
        current_.daytime = input.daytime;
        current_.sunEnabled = input.sunEnabled;
        return current_;
    }

private:
    ScreenSceneSnapshot current_{};
    bool initialized_ = false;
};

}
