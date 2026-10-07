#pragma once

namespace bl::runtime::content {

constexpr int kCustomSoundMarkerId = 1000;

int registerSoundStyle(int soundId, float volume, float pitch, float pitchVariance,
                       int maxInstances, int limitBehavior);
int playSoundStyle(int style, int x, int y, float volumeScale, float pitchOffset);
int findActiveSoundStyle(int style);
bool installNativeSoundRouter();

} // namespace bl::runtime::content
