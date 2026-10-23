#include "experiments/csharp/WorldQuitSaveFence.h"
#include "experiments/csharp/NativeWorldStore.h"
#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/ManagedRuntime.h"
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

using namespace bl::csharp::worldsave;
namespace {
std::vector<uint8_t> fixture;
std::atomic<int> saves{0}, confirmations{0}, unloads{0};
}
namespace bl::csharp {
bool requestManagedWorldOnMono(const MonoWorldRequest& request, MonoWorldReply* reply, int) {
    reply->success = true;
    switch (request.action) {
        case MonoWorldAction::Enter: return true;
        case MonoWorldAction::Prepare: ++saves; return encodeWorldBase64(fixture, &reply->payload);
        case MonoWorldAction::Confirm: ++confirmations; return true;
        case MonoWorldAction::Leave: ++unloads; return true;
        default: return false;
    }
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    fixture.assign(std::istreambuf_iterator<char>(input), {});
    char temp[] = "/tmp/bunny-quit-save-XXXXXX";
    const char* folder = mkdtemp(temp);
    assert(folder);
    const std::string world = std::string(folder) + "/qa.wld";
    { std::ofstream(world) << "vanilla-world"; }
    auto& store = activeWorldStore();
    assert(store.beginWorld(71, world, false));
    std::string error;
    assert(enterManagedWorld(71, true, &error));
    WorldQuitSaveFence fence;
    std::mutex transition, rendezvous;
    std::condition_variable ready;
    bool menuChanged = false, observerRan = false;
    std::thread game([&] {
        std::unique_lock wait(rendezvous);
        ready.wait(wait, [&] { return menuChanged; });
        wait.unlock();
        // Exercise the teardown decision AFTER the Mobile original switched
        // to the menu, but BEFORE it reaches InternalSaveWorld.
        { std::lock_guard session(transition);
          if (!fence.active()) { leaveManagedWorld(71, &error); store.endWorld(71); }
        }
        wait.lock(); observerRan = true; wait.unlock(); ready.notify_one();
    });
    {
        WorldQuitSaveFence::Scope saving(fence, transition);
        { std::lock_guard lock(rendezvous); menuChanged = true; }
        ready.notify_one();
        { std::unique_lock wait(rendezvous); ready.wait(wait, [&] { return observerRan; }); }
        assert(store.active(71) && unloads == 0);
        // Real native coordinator/store: the sidecar must still be writable.
        assert(saveManagedWorld(71, true, &error));
        assert(saves == 1 && confirmations == 1);
        assert(leaveManagedWorld(71, &error) && unloads == 1);
    }
    game.join();
    assert(!fence.active());
    { std::lock_guard session(transition); store.endWorld(71); }
    assert(store.beginWorld(72, world, false));
    std::vector<uint8_t> restored;
    assert(store.read(72, &restored) == Result::Ok && restored == fixture);
    store.endWorld(72);
    std::filesystem::remove_all(folder);
    std::puts("MOBILE_MENU_BEFORE_SAVE_PRESERVES_WORLD_SIDECAR_PASS");
}
