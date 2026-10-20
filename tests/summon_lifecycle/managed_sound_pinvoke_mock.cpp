#include "experiments/csharp/ManagedSoundLoader.cpp"
#include <cassert>
#include <set>
#include <string>

namespace bl::log {
void write(int, const char*, ...) {}
}

namespace {
bl::mods::LoadedMod currentMod;
int nextStyle=0;
int nextToken=0;
int stopCount=0;
int loadCount=0;
int lastX=0, lastY=0;
std::set<int> styles;
}

namespace bl::mods {
const LoadedMod* find(const std::string& id) {
    return id==currentMod.id && currentMod.enabled ? &currentMod : nullptr;
}
}

namespace bl::runtime::content {
int loadSound(const std::string& path) {
    assert(path.ends_with(".ogg"));
    ++loadCount;
    return 34;
}
int soundState(int id) {
    assert(id==34);
    return 1;
}
int soundDuration(int id) {
    assert(id==34);
    return 550;
}
int registerSoundStyle(int soundId, float volume, float pitch, float variance,
                       int maxInstances, int behavior) {
    assert(soundId==34 && volume>=0 && volume<=4 && pitch>=-1 && pitch<=1);
    assert(variance>=0 && variance<=2 && maxInstances>=0);
    assert(behavior==0 || behavior==1);
    int style=++nextStyle;
    styles.insert(style);
    return style;
}
int playSoundStyle(int style,int x,int y,float volume,float pitch) {
    assert(styles.contains(style));
    assert(volume>0 && volume<=4 && pitch>=-1 && pitch<=1);
    lastX=x;lastY=y;
    return kAsyncSoundTokenBase+(++nextToken);
}
int findActiveSoundStyle(int style) {
    assert(styles.contains(style));
    return kAsyncSoundTokenBase+nextToken;
}
void stopSoundStyleToken(int token) {
    assert(token>=kAsyncSoundTokenBase);
    ++stopCount;
}
void releaseSoundStyle(int style) {
    assert(styles.erase(style)==1);
}
void stopAllSoundStyleInstances(int style) {
    assert(styles.contains(style));
    ++stopCount;
}
}

extern "C" {
__attribute__((visibility("default"))) void bunny_sound_test_mount(const char* root) {
    assert(root && *root);
    currentMod.id="sound-pinvoke-fixture";
    currentMod.dir=root;
    currentMod.enabled=true;
}
__attribute__((visibility("default"))) void bunny_sound_test_pump() {
    bl::csharp::managedSound::pump();
}
__attribute__((visibility("default"))) void bunny_sound_test_world_exit() {
    bl::csharp::managedSound::worldExit();
}
__attribute__((visibility("default"))) int bunny_sound_test_stops() { return stopCount; }
__attribute__((visibility("default"))) int bunny_sound_test_loads() { return loadCount; }
__attribute__((visibility("default"))) int bunny_sound_test_x() { return lastX; }
__attribute__((visibility("default"))) int bunny_sound_test_y() { return lastY; }
}
