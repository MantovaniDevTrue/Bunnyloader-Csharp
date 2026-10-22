#define BUNNY_SURFACE_LOADER_LIBRARY_ONLY
#define BUNNY_TEST_EXTERNAL_STAGE_BRIDGE
#define BUNNY_TEST_EXTERNAL_TEXTURE_LOOKUP
#include "native_surface_loader_test.cpp"
#include "experiments/csharp/ManagedSurfaceBackgrounds.cpp"
#include "experiments/csharp/UniversalHookMailbox.h"
#include <condition_variable>

namespace {
bl::csharp::UniversalHookMailbox surfaceMailbox;
std::mutex surfaceWakeMutex;
std::condition_variable surfaceWakeChanged;
bool surfaceWakePending=false,surfaceStop=false;
std::thread surfaceWorker;
std::atomic<int> surfaceWorkerCalls=0,surfaceOriginalCalls=0;
int surfaceManagedSlot=-1;
void wakeSurfaceWorker(){std::lock_guard lock(surfaceWakeMutex);surfaceWakePending=true;surfaceWakeChanged.notify_one();}
void dispatchSurfaceWorker(){
    const auto result=surfaceMailbox.dispatch(surfaceManagedSlot,0,[]{++surfaceOriginalCalls;return true;},wakeSurfaceWorker,std::chrono::seconds(2));
    CHECK(result.accepted && !result.faulted && result.originalRan);
}
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_stage(const char* name,int slot) {
    if(std::strcmp(name,"SurfaceBackground"))return -3;
    surfaceManagedSlot=slot;bl::csharp::managedSurfaceDispatch(dispatchSurfaceWorker);return 1013;
}
extern "C" __attribute__((visibility("default"))) void bunny_universal_hook_remove(int handle) {
    CHECK(handle==1013);bl::csharp::removeManagedSurfaceDispatch();
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_original(int slot,int64_t ticket) {
    return surfaceMailbox.original(slot,uint64_t(ticket))?1:0;
}
extern "C" __attribute__((visibility("default"))) int bunny_background_texture_slot(const char*,const char* path) {
    const std::string p=path;
    if(p.ends_with("ExampleBiomeSurfaceFar"))return 10;
    if(p.ends_with("ExampleBiomeSurfaceClose"))return 15;
    for(int i=0;i<4;++i)if(p.ends_with("ExampleBiomeSurfaceMid"+std::to_string(i)))return 11+i;
    return -1;
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_initialize() {
    configureLoader();std::string error;return surface::install(&error)?1:0;
}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_callback(int(*handler)(int,int64_t)) {
    managedCallback=handler;
    surfaceWorker=std::thread([]{
        for(;;) {
            {std::unique_lock lock(surfaceWakeMutex);surfaceWakeChanged.wait(lock,[]{return surfaceWakePending || surfaceStop;});if(surfaceStop)return;surfaceWakePending=false;}
            bl::csharp::UniversalHookMailbox::Work work{};if(!surfaceMailbox.take(work))continue;
            CHECK(gettid()!=gameTid);++surfaceWorkerCalls;
            int result=managedCallback(work.slot,int64_t(work.ticket));
            CHECK(surfaceMailbox.complete(work.ticket,result>0));
        }
    });
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_pick(int vanilla){return runPreferred(vanilla);}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_scene(int slot,int menuValue) {
    shownStyle=slot;set(menu,menuValue!=0);CHECK(runPreferred(0)>=0);
    alpha(slot,true,.5f);alpha(slot,false,.5f);
}
extern "C" __attribute__((visibility("default"))) float bunny_test_surface_fades(int slot,float speed) {
    foreground(nullable(true,slot),nullable(true,speed));return static_cast<float*>(arrayData(fade(true)))[slot];
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_layer(int layer) {
    actualTextures.clear();actualDraws.clear();
    if(layer==0)drawFar();else if(layer==1)drawMiddle();else {drawFar();actualTextures.clear();actualDraws.clear();CHECK(drawClose()==.75f);}
    return actualTextures.empty()?-1:actualTextures.back();
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_draw_count(){return int(actualDraws.size());}
extern "C" __attribute__((visibility("default"))) double bunny_test_surface_draw_value(int index) {
    return actualDraws.empty() || index<0 || index>=6?-999:actualDraws.back()[size_t(index)];
}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_reset(){surface::resetWorld();}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_calls(){return surfaceWorkerCalls.load();}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_cleanup() {
    {std::lock_guard lock(surfaceWakeMutex);surfaceStop=true;surfaceWakeChanged.notify_one();}
    surfaceWorker.join();CHECK(surfaceWorkerCalls==surfaceOriginalCalls);
    for(auto& [id,e]:surface::entries){(void)id;for(auto& layer:e.layers)surface::release(layer);}
    for(int slot=10;slot<=15;++slot)bl::runtime::backgroundAssets::retire(slot);
    bl::runtime::backgroundAssets::sweep();CHECK(handles.empty());
    for(void* value:allocations)std::free(value);
    allocations.clear();
}
