#include "experiments/csharp/NativeInvasionNotifications.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "menu/NetRequests.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {

struct Notice {
    uint64_t session = 0;
    std::string mod;
    std::string id;
    int phase = 0;
    int progress = 0;
    int goal = 0;
};

std::mutex g_mutex;
std::deque<Notice> g_queue;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};

bool nameOk(const char* value, size_t max) {
    if (!value) return false;
    const std::string s(value);
    if (s.empty() || s.size() > max) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

const MethodInfo* textMethod() {
    static const MethodInfo* method = nullptr;
    if (method) return method;
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    method = main ? il2cpp::api().class_get_method_from_name(main, "NewText", 5) : nullptr;
    return method;
}
}

void setNativeInvasionNoticeWorld(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready))
        g_queue.clear();
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session != 0, std::memory_order_release);
}

void pumpNativeInvasionNoticesOnGameThread() {
    if (!g_ready.load(std::memory_order_acquire) ||
        runtime::netMode() == 2) return;
    const MethodInfo* method = textMethod();
    if (!method) return;
    for (int index = 0; index < 2; ++index) {
        Notice notice;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_queue.empty()) break;
            notice = std::move(g_queue.front());
            g_queue.pop_front();
        }
        if (notice.session != g_session.load(std::memory_order_acquire)) continue;
        std::string message = "[Invasion] " + notice.id + ": ";
        if (notice.phase == 2) message += "100% - complete!";
        else if (notice.phase == 0) message += "stopped";
        else if (notice.progress == 0) message += "started";
        else message += std::to_string(notice.progress * 100 / notice.goal) + "%";
        auto& api = il2cpp::api();
        Il2CppString* text = api.string_new(message.c_str());
        uint8_t red = 250, green = 205, blue = 90;
        bool localOnly = false;
        void* args[] = {text, &red, &green, &blue, &localOnly};
        Il2CppObject* error = nullptr;
        api.runtime_invoke(method, nullptr, args, &error);
        if (error) return;
    }
}

}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_notice(int64_t session, const char* mod, const char* id,
                          int phase, int progress, int goal) {
    using namespace bl::csharp;
    if (session <= 0 || !nameOk(mod, 128) || !nameOk(id, 64) ||
        phase < 0 || phase > 2 || goal <= 0 || goal > 1000000 ||
        progress < 0 || progress > goal) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    for (Notice& notice : g_queue) {
        if (notice.session == static_cast<uint64_t>(session) &&
            notice.mod == mod && notice.id == id) {
            notice.phase = phase;
            notice.progress = progress;
            notice.goal = goal;
            return 1;
        }
    }
    if (g_queue.size() >= 64) return 0;
    g_queue.push_back({static_cast<uint64_t>(session), mod, id,
                       phase, progress, goal});
    return 1;
}
