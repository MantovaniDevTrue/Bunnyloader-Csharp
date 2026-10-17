#pragma once

namespace bl::graphics {

struct ScreenSceneRaw {
    bool dayTime = true;
    double gameTime = 0;
    double worldSurface = 250;
    float cameraX = 0;
    float cameraY = 0;
    float playerCenterY = 0;
    int screenWidth = 1920;
    int screenHeight = 1080;
    int moonPhase = 0;
    bool invertedGravity = false;
    int skyRed = 0, skyGreen = 0, skyBlue = 0;
    bool snow = false, crimson = false, corrupt = false;
    int snowBackground = 0, desertBackground = 0;
    float snowAlpha = 0, desertAlpha = 0;
};

struct ScreenSceneSnapshot {
    ScreenSceneRaw raw;
    bool valid = false;
    bool daytime = true;
    bool sunEnabled = true;
    float sunX = 0.5f;
    float sunY = 0.75f;
    float clock = 0.5f;
    float strength = 0.6f;
    float shadowThreshold = 0.98f;
    float dayBlend = 1.0f;
    float sunVisibility = 1.0f;
};

ScreenSceneSnapshot sampleScreenScene();
void resetScreenScene();

}
