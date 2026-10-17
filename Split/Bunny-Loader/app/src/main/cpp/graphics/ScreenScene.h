#pragma once

namespace bl::graphics {

struct ScreenSceneSnapshot {
    bool valid = false;
    bool daytime = true;
    bool sunEnabled = true;
    float sunX = 0.5f;
    float sunY = 0.75f;
    float clock = 0.5f;
    float strength = 0.6f;
    float shadowThreshold = 0.98f;
};

ScreenSceneSnapshot sampleScreenScene();

}
