#include "experiments/csharp/NativeWorldSystemPersistence.h"
#include "experiments/csharp/WorldSystemSaveFile.h"

#include <cstdint>
#include <cstring>
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

void setNativeWorldSystemSaveWorld(bool ready, uint64_t session,
                                   const std::string& path, bool cloud) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_session = session;
    g_ready = ready && session > 0 && !cloud &&
        path.size() > 4 && path.size() <= 2048 &&
        path.substr(path.size() - 4) == ".wld" &&
        path.find("..") == std::string::npos;
    g_path = g_ready ? path + ".worldsystems.bl" : std::string();
}

}

extern "C" __attribute__((visibility("default")))
int bunny_world_system_state_read(int64_t session, const char* mod,
                                   const char* system, char* output,
                                   int capacity) {
    using namespace bl::csharp;
    if (!mod || !system || !output || capacity < 2 || capacity > 16385)
        return -1;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!active(session)) return -2;
    std::string payload;
    int result = world_save::readState(g_path, mod, system, &payload);
    if (result != 1) return result;
    if (payload.size() >= static_cast<size_t>(capacity)) return -1;
    std::memcpy(output, payload.data(), payload.size());
    output[payload.size()] = '\0';
    return static_cast<int>(payload.size());
}

extern "C" __attribute__((visibility("default")))
int bunny_world_system_state_write(int64_t session, const char* mod,
                                    const char* system, const char* payload) {
    using namespace bl::csharp;
    if (!mod || !system || !payload) return -1;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!active(session)) return -2;
    const size_t size = strnlen(payload, 16385);
    if (size > 16384) return -1;
    return world_save::writeState(g_path, mod, system,
                                  std::string(payload, size)) ? 1 : -1;
}
