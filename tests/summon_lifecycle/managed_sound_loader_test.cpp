#include "experiments/csharp/ManagedSoundLoader.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <thread>

namespace bl::log { void write(int,const char*,...) {} }

namespace bl::mods {
LoadedMod modA, modB;
const LoadedMod* find(const std::string& id) {
    if (id==modA.id) return &modA;
    if (id==modB.id) return &modB;
    return nullptr;
}
}

namespace bl::runtime::content {
bool ready = false, decoderFailure = false, nativePump = false;
int loads = 0, registrations = 0, plays = 0, stopped = 0, released = 0, stoppedWorld = 0;
int nextToken = kAsyncSoundTokenBase + 20;
std::set<int> activeStyles;
int loadSound(const std::string& path) {
    assert(nativePump);
    assert(std::filesystem::exists(path));
    ++loads; return 7;
}
int soundState(int id) {
    assert(nativePump);
    assert(id==7); return decoderFailure?-1:ready?1:0;
}
int soundDuration(int id) {assert(id==7); return 312;}
int registerSoundStyle(int id,float volume,float pitch,float variance,int maximum,int behavior) {
    assert(nativePump);
    assert(id==7&&volume>=0&&volume<=4&&pitch>=-1&&pitch<=1);
    assert(variance>=0&&variance<=2&&maximum>=0&&maximum<=256);
    assert(behavior==0||behavior==1);
    const int style=++registrations;
    activeStyles.insert(style);
    return style;
}
int playSoundStyle(int style,int x,int y,float volume,float pitch) {
    assert(nativePump);
    assert(activeStyles.contains(style));
    assert(volume>=0&&volume<=4&&pitch>=-1&&pitch<=1);
    assert((x==-1&&y==-1)||(x>=0&&y>=0));
    ++plays;
    return ++nextToken;
}
int findActiveSoundStyle(int style) {
    assert(nativePump&&activeStyles.contains(style));
    return nextToken;
}
void stopSoundStyleToken(int token) {
    assert(nativePump&&token>=kAsyncSoundTokenBase);
    ++stopped;
}
void releaseSoundStyle(int style) {
    assert(nativePump);
    assert(activeStyles.erase(style)==1);
    ++released;
}
void stopAllSoundStyleInstances(int style) {
    assert(nativePump&&activeStyles.contains(style));
    ++stoppedWorld;
}
}

void runPump() {
    bl::runtime::content::nativePump=true;
    bl::csharp::managedSound::pump();
    bl::runtime::content::nativePump=false;
}

void exitWorld() {
    bl::runtime::content::nativePump=true;
    bl::csharp::managedSound::worldExit();
    bl::runtime::content::nativePump=false;
}

int main() {
    namespace fs=std::filesystem;
    using namespace bl::csharp::managedSound;
    using namespace bl::runtime::content;
    const auto dir=fs::temp_directory_path()/(
        "bunny-csharp-sound-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir/"a"/"content"/"Assets"/"Sounds");
    fs::create_directories(dir/"b"/"content"/"Assets"/"Sounds");
    fs::create_directories(dir/"outside");
    {
        std::ofstream(dir/"a"/"content"/"Assets"/"Sounds"/"hit.ogg")<<"test audio a";
        std::ofstream(dir/"b"/"content"/"Assets"/"Sounds"/"hit.ogg")<<"test audio b";
        std::ofstream(dir/"outside"/"private.ogg")<<"private outside";
    }
    bl::mods::modA.id="example-a";
    bl::mods::modA.dir=(dir/"a").string();
    bl::mods::modB.id="example-b";
    bl::mods::modB.dir=(dir/"b").string();

    assert(registerClip("unknown","Assets/Sounds/hit.ogg",1,0,0,2,1)==-1);
    assert(registerClip("example-a","../outside/private.ogg",1,0,0,2,1)==-1);
    assert(registerClip("example-a","Assets/Sounds/hit.png",1,0,0,2,1)==-1);
    assert(registerClip("example-a","Assets/Sounds/missing.ogg",1,0,0,2,1)==-1);
    fs::create_symlink(dir/"outside"/"private.ogg",dir/"a"/"content"/"escape.ogg");
    assert(registerClip("example-a","escape.ogg",1,0,0,2,1)==-1);
    assert(registerClip("example-a","Assets/Sounds/hit.ogg",
        std::numeric_limits<float>::quiet_NaN(),0,0,2,1)==-1);
    assert(registerClip("example-a","Assets/Sounds/hit.ogg",1,0,0,257,1)==-1);

    constexpr const char* path="Assets/Sounds/hit.ogg";
    int a=registerClip("example-a",path,0.75f,0,0.12f,2,0);
    int b=registerClip("example-b",path,0.75f,0,0.12f,2,0);
    assert(a>0&&b>0&&b!=a);
    assert(registerClip("example-a",path,0.75f,0,0.12f,2,0)==a);
    std::vector<int> duplicates(24,-1);
    std::vector<std::thread> threads;
    for(int i=0;i<24;++i)
        threads.emplace_back([i,&duplicates] {
            duplicates[i]=registerClip("example-a",path,0.75f,0,0.12f,2,0);
        });
    for(auto& worker:threads)worker.join();
    for(int handle:duplicates)assert(handle==a);
    assert(loads==0&&registrations==0);
    assert(state(a)==0&&duration(a)==0);
    assert(play(a,1,-1,1,0)==-1);
    assert(play(a,-1,-1,-1,0)==-1);

    int first=play(a,160,160,1,0), cancelled=play(a,160,160,1,0);
    assert(first>0&&cancelled>0);
    assert(stop(a,cancelled)==1);
    assert(playback(a,first)==0&&playback(a,cancelled)==-1);
    runPump();
    assert(loads==2&&registrations==0&&state(a)==0&&plays==0);
    ready=true;
    runPump();
    assert(state(a)==1&&state(b)==1&&registrations==2);
    assert(playback(a,first)>kAsyncSoundTokenBase&&plays==1);
    assert(playback(b,first)==-1);
    assert(duration(a)==312);
    int stream=playback(a,first);
    assert(lastActive(a)==stream);
    assert(stop(a,first)==1&&playback(a,first)==-1);
    runPump();assert(stopped==1);

    int worldRequest=play(a,-1,-1,1,0);
    runPump();assert(playback(a,worldRequest)>0);
    exitWorld();
    assert(stoppedWorld==2&&playback(a,worldRequest)==-1);
    assert(state(a)==1);
    int afterWorld=play(a,-1,-1,1,0);
    runPump();assert(playback(a,afterWorld)>0);

    unloadMod("example-a");
    assert(state(a)==-2&&state(b)==1);
    int old=play(a,-1,-1,1,0);
    assert(old==-1);
    runPump();
    assert(state(a)==-2&&released==1&&state(b)==1);
    assert(activeStyles.size()==1);

    int newA=registerClip("example-a",path,0.5f,0,0.1f,1,1);
    assert(newA>a);
    runPump();
    assert(state(newA)==1);
    int active=play(newA,-1,-1,1,0);
    runPump();assert(playback(newA,active)>0);
    assert(plays>=4);

    // The per-owner limit cannot be bypassed by changing pitch/volume.
    for(int i=0;i<31;++i) {
        const float volume=0.01f * (i+1);
        assert(registerClip("example-a",path,volume,0,0,1,1)>0);
    }
    assert(registerClip("example-a",path,0.99f,0,0,1,1)==-1);
    assert(state(b)==1);
    unloadMod("example-a");
    unloadMod("example-b");
    runPump();
    assert(released>=3&&activeStyles.empty());
    assert(clips.empty());

    fs::remove_all(dir);
    std::cout<<"CSHARP_SOUNDLOADER_OWNER_ISOLATION_ASSET_CONFINEMENT_ASYNC_DECODE_BOUNDED_QUEUE_CANCEL_WORLD_EXIT_UNLOAD_LIMITS_CONCURRENCY_PASS\n";
}
