#pragma once

namespace bl::runtime::content {

constexpr int kCustomSoundMarkerId = 1000;
constexpr int kAsyncSoundTokenBase = 0x40000000;

int registerSoundStyle(int soundId, float volume, float pitch, float pitchVariance,
                       int maxInstances, int limitBehavior);
// Releases a C#-owned style without changing any surviving style ID.
void releaseSoundStyle(int style);
int playSoundStyle(int style, int x, int y, float volumeScale, float pitchOffset);
int findActiveSoundStyle(int style);
void stopSoundStyleToken(int token);
void stopAllSoundStyleInstances(int style);
bool isSoundStyleToken(int token);
bool installNativeSoundRouter();

}
