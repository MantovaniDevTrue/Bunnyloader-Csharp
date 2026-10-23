#include "experiments/csharp/NativeInvasionPersistence.h"
#include "experiments/csharp/InvasionSaveFile.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {

std::mutex g_mutex;
uint64_t g_session = 0;
std::string g_path;
bool g_ready = false;

bool active(int64_t session) {
    return session > 0 && g_ready &&
        g_session == static_cast<uint64_t>(session) && !g_path.empty();
}

}

void setNativeInvasionSaveWorld(bool ready, uint64_t session,
                                const std::string& path, bool cloud) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_session = session;
    g_ready = ready && session > 0 && !cloud &&
        path.size() > 4 && path.size() <= 2048 &&
        path.substr(path.size() - 4) == ".wld" &&
        path.find("..") == std::string::npos;
    g_path = g_ready ? path + ".invasions.bl" : std::string();
}

}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_state_read(int64_t session, const char* mod, const char* id,
                              int* phase, int* progress, int* cooldown) {
    using namespace bl::csharp;
    if (!mod || !id || !phase || !progress || !cooldown) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!active(session)) return 0;
    invasion_save::State state;
    if (!invasion_save::readState(g_path, mod, id, &state)) return 0;
    *phase = state.phase;
    *progress = state.progress;
    *cooldown = state.cooldown;
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_state_write(int64_t session, const char* mod, const char* id,
                               int phase, int progress, int cooldown) {
    using namespace bl::csharp;
    if (!mod || !id) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!active(session)) return 0;
    return invasion_save::writeState(
        g_path, mod, id, {phase, progress, cooldown}) ? 1 : 0;
}
