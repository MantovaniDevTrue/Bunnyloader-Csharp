#include "experiments/csharp/UniversalHooks.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/GuardedCall.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <utility>
#include <unistd.h>
#include <sys/syscall.h>

namespace bl::csharp {
namespace {
constexpr int MaxHooks = 64;
struct Entry {
    const MethodInfo* method = nullptr;
    void* original = nullptr;
    bool isStatic = false;
    int managedSlot = -1;
    std::atomic<bool> active{false};
    std::chrono::steady_clock::time_point retryAfter{};
    std::chrono::steady_clock::time_point timeoutWindow{};
    int timeoutsInWindow = 0;
    bool suspended = false;
};
std::array<Entry, MaxHooks> entries;
int used = 0;
std::atomic<long> gameThread{0};
thread_local bool inOriginal = false;
struct Install {
    std::mutex mutex;
    std::condition_variable wake;
    bool pending = false, done = false;
    uint64_t epoch = 0;
    std::string type, method;
    int slot = -1, result = -1;
    bool isStatic = false;
} install;
std::atomic<uint64_t> epoch{1};
std::atomic<bool> accepting{true};
std::array<std::atomic<int64_t>, 9> counters{};

void callOriginal(Entry& entry, uintptr_t self, const MethodInfo* info) {
    void* target = __atomic_load_n(&entry.original, __ATOMIC_ACQUIRE);
    if (entry.isStatic) reinterpret_cast<void(*)(const MethodInfo*)>(target)(info);
    else reinterpret_cast<void(*)(void*, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), info);
}
void dispatch(int index, uintptr_t self, const MethodInfo* info) {
    Entry& entry = entries[index];
    if (!entry.active.load(std::memory_order_acquire) || inOriginal || universalNativeAccessInProgress ||
        static_cast<long>(syscall(__NR_gettid)) != gameThread.load(std::memory_order_acquire)) {
        callOriginal(entry, self, info);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now < entry.retryAfter) {
        callOriginal(entry, self, info);
        return;
    }
    if (entry.suspended) {
        entry.suspended = false;
        entry.timeoutsInWindow = 0;
        entry.timeoutWindow = now;
        counters[8].fetch_add(1, std::memory_order_relaxed);
    }
    auto original = [&] {
        inOriginal = true;
        auto body = [&] { callOriginal(entry, self, info); };
        const bool ok = il2cpp::guarded(body);
        inOriginal = false;
        return ok;
    };
    counters[0].fetch_add(1, std::memory_order_relaxed);
    const auto result = universalHookMailbox().dispatch(entry.managedSlot, self,
                                                       original, wakeUniversalHookWorker,
                                                       std::chrono::milliseconds(3), universalNativeService.load(std::memory_order_acquire));
    if (!result.accepted) counters[2].fetch_add(1, std::memory_order_relaxed);
    else if (!result.faulted) counters[1].fetch_add(1, std::memory_order_relaxed);
    if (result.timedOut) counters[3].fetch_add(1, std::memory_order_relaxed);
    if (result.faulted) {
        bool disable = true;
        if (result.timedOut) {
            const auto now = std::chrono::steady_clock::now();
            if (now - entry.timeoutWindow > std::chrono::seconds(30)) {
                entry.timeoutWindow = now;
                entry.timeoutsInWindow = 0;
            }
            if (++entry.timeoutsInWindow < 3) {
                entry.retryAfter = now + std::chrono::seconds(1);
                counters[5].fetch_add(1, std::memory_order_relaxed);
            } else {
                entry.suspended = true;
                entry.retryAfter = now + std::chrono::seconds(30);
                counters[7].fetch_add(1, std::memory_order_relaxed);
            }
            disable = false;
        }
        if (disable) {
            counters[4].fetch_add(1, std::memory_order_relaxed);
            entry.active.store(false, std::memory_order_release);
        }
    }
    if ((!result.accepted || result.faulted) && !result.originalRan) callOriginal(entry, self, info);
}
template<size_t N> void instanceStub(void* self, const MethodInfo* info) {
    dispatch(N, reinterpret_cast<uintptr_t>(self), info);
}
template<size_t N> void staticStub(const MethodInfo* info) { dispatch(N, 0, info); }
template<size_t... N> auto instanceStubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceStub<N>)...};
}
template<size_t... N> auto staticStubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticStub<N>)...};
}
int installOnGameThread() {
#if !defined(__aarch64__) && !defined(BUNNY_UNIVERSAL_HOOK_HOST_TEST)
    return -2;
#else
    auto& api = il2cpp::api();
    const size_t dot = install.type.rfind('.');
    if (dot == std::string::npos || install.type.rfind("Terraria.", 0) != 0 ||
        install.method.empty() || install.method.front() == '.' ||
        !accepting.load(std::memory_order_acquire) || install.epoch != epoch.load(std::memory_order_acquire)) return -3;
    if (!api.class_get_methods || !api.method_get_param_count || !api.method_get_name ||
        !api.method_get_return_type || !api.type_get_name || !api.il2cpp_free ||
        !api.class_is_valuetype || !api.method_is_instance || !api.method_is_generic || !api.class_is_generic) return -4;
    Il2CppClass* cls = il2cpp::findClassQuiet(install.type.substr(0, dot), install.type.substr(dot + 1));
    if (!cls || api.class_is_valuetype(cls) || api.class_is_generic(cls)) return -5;
    const MethodInfo* selected = nullptr;
    void* iter = nullptr;
    while (const MethodInfo* method = api.class_get_methods(cls, &iter)) {
        if (install.method != api.method_get_name(method) || api.method_get_param_count(method) != 0 ||
            api.method_is_instance(method) == install.isStatic) continue;
        char* name = api.type_get_name(api.method_get_return_type(method));
        const bool isVoid = name && std::strcmp(name, "System.Void") == 0;
        if (name) api.il2cpp_free(name);
        if (!isVoid || selected || api.method_is_generic(method)) return -6;
        selected = method;
    }
    if (!selected || !il2cpp::methodPointer(selected)) return -7;
    for (int i = 0; i < used; ++i) {
        auto& entry = entries[i];
        if (il2cpp::methodPointer(entry.method) != il2cpp::methodPointer(selected)) continue;
        if (entry.active.load(std::memory_order_acquire) || entry.method != selected || entry.isStatic != install.isStatic) return -8;
        entry.managedSlot = install.slot;
        entry.retryAfter = {};
        entry.timeoutWindow = {};
        entry.timeoutsInWindow = 0;
        entry.suspended = false;
        entry.active.store(true, std::memory_order_release);
        return i;
    }
    if (used >= MaxHooks) return -3;
    static const auto instances = instanceStubs(std::make_index_sequence<MaxHooks>{});
    static const auto statics = staticStubs(std::make_index_sequence<MaxHooks>{});
    Entry& entry = entries[used];
    entry.method = selected;
    entry.isStatic = install.isStatic;
    entry.managedSlot = install.slot;
    entry.retryAfter = {};
    entry.timeoutWindow = {};
    entry.timeoutsInWindow = 0;
    entry.suspended = false;
    if (!hook::install(selected, entry.isStatic ? statics[used] : instances[used], &entry.original)) return -9;
    entry.active.store(true, std::memory_order_release);
    return used++;
#endif
}
}
void pumpUniversalHookInstallsOnGameThread() {
    gameThread.store(static_cast<long>(syscall(__NR_gettid)), std::memory_order_release);
    std::unique_lock<std::mutex> lock(install.mutex, std::try_to_lock);
    if (!lock || !install.pending) return;
    install.result = installOnGameThread();
    install.pending = false;
    install.done = true;
    install.wake.notify_one();
}
void disableUniversalHooks() {
    std::lock_guard<std::mutex> lock(install.mutex);
    accepting.store(false, std::memory_order_release);
    epoch.fetch_add(1, std::memory_order_acq_rel);
    universalHookMailbox().cancel();
    for (auto& entry : entries) entry.active.store(false, std::memory_order_release);
    install.pending = false;
    install.result = -10;
    install.done = true;
    install.wake.notify_one();
}
void enableUniversalHooks() { accepting.store(true, std::memory_order_release); }

extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install(
    const char* type, const char* method, int slot, int isStatic) {
    if (!accepting.load(std::memory_order_acquire) || !type || !method || strnlen(type, 257) > 256 || strnlen(method, 129) > 128 ||
        slot < 0 || slot >= MaxHooks || (isStatic != 0 && isStatic != 1)) return -1;
    if (!universalCanInstall || !universalLeaveMono || !universalResumeMono) return -12;
    UniversalHookWorkerYield yield;
    std::unique_lock<std::mutex> lock(install.mutex);
    if (install.pending) return -1;
    install.type = type;
    install.method = method;
    install.slot = slot;
    install.isStatic = isStatic != 0;
    install.epoch = epoch.load(std::memory_order_acquire);
    install.pending = true;
    install.done = false;
    if (!install.wake.wait_for(lock, std::chrono::seconds(2), [] { return install.done; })) {
        install.pending = false;
        return -11;
    }
    return install.result;
}
extern "C" __attribute__((visibility("default"))) void bunny_universal_hook_remove(int index) {
    if (index >= 0 && index < MaxHooks) entries[index].active.store(false, std::memory_order_release);
}
extern "C" __attribute__((visibility("default"))) int64_t bunny_universal_hook_stat(int key) {
    if (key == 6) return universalHookLateCompletionCount();
    return key >= 0 && key < 9 ? counters[key].load(std::memory_order_relaxed) : -1;
}
#if defined(BUNNY_UNIVERSAL_HOOK_HOST_TEST)
extern "C" void bunny_universal_hook_test_expire_suspend(int index) {
    if (index >= 0 && index < used) entries[index].retryAfter = {};
}
#endif
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_original(int slot, int64_t ticket) {
    return universalHookMailbox().original(slot, static_cast<uint64_t>(ticket)) ? 1 : 0;
}
}
