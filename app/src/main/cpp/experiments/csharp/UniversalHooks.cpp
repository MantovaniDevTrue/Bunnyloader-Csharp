#include "experiments/csharp/UniversalHooks.h"
#include "experiments/csharp/UniversalSystemStages.h"
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
    bool int32Mode = false;
    bool int32PairMode = false;
    int extendedMode = 0;
    int managedSlot = -1;
    std::atomic<bool> active{false};
    std::chrono::steady_clock::time_point retryAfter{};
    std::chrono::steady_clock::time_point timeoutWindow{};
    int timeoutsInWindow = 0;
    int suspensionsInBurst = 0;
    int lastSuspensionSeconds = 0;
    std::chrono::steady_clock::time_point lastTimeout{};
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
    bool isStatic = false, int32Mode = false, int32PairMode = false;
    bool systemStage = false;
    int extendedMode = 0;
} install;
std::atomic<uint64_t> epoch{1};
std::atomic<bool> accepting{true};
std::array<std::atomic<int64_t>, 10> counters{};

void callOriginal(Entry& entry, uintptr_t self, const MethodInfo* info) {
    void* target = __atomic_load_n(&entry.original, __ATOMIC_ACQUIRE);
    if (entry.isStatic) reinterpret_cast<void(*)(const MethodInfo*)>(target)(info);
    else reinterpret_cast<void(*)(void*, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), info);
}
int32_t callOriginalInt32(Entry& entry, uintptr_t self, int32_t value, const MethodInfo* info) {
    void* target = __atomic_load_n(&entry.original, __ATOMIC_ACQUIRE);
    if (entry.isStatic)
        return reinterpret_cast<int32_t(*)(int32_t, const MethodInfo*)>(target)(value, info);
    return reinterpret_cast<int32_t(*)(void*, int32_t, const MethodInfo*)>(target)(
        reinterpret_cast<void*>(self), value, info);
}
int32_t callOriginalInt32Pair(Entry& entry, uintptr_t self, int32_t first, int32_t second, const MethodInfo* info) {
    void* target = __atomic_load_n(&entry.original, __ATOMIC_ACQUIRE);
    if (entry.isStatic)
        return reinterpret_cast<int32_t(*)(int32_t, int32_t, const MethodInfo*)>(target)(first, second, info);
    return reinterpret_cast<int32_t(*)(void*, int32_t, int32_t, const MethodInfo*)>(target)(
        reinterpret_cast<void*>(self), first, second, info);
}
int32_t callOriginalExtended(Entry& entry, uintptr_t self, int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    void* target = __atomic_load_n(&entry.original, __ATOMIC_ACQUIRE);
    switch (entry.extendedMode) {
        case 1:
            if (entry.isStatic) return reinterpret_cast<int32_t(*)(const MethodInfo*)>(target)(info);
            return reinterpret_cast<int32_t(*)(void*, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), info);
        case 2:
            if (entry.isStatic) reinterpret_cast<void(*)(int32_t, const MethodInfo*)>(target)(a, info);
            else reinterpret_cast<void(*)(void*, int32_t, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), a, info);
            return 0;
        case 3:
            if (entry.isStatic) reinterpret_cast<void(*)(int32_t, int32_t, const MethodInfo*)>(target)(a, b, info);
            else reinterpret_cast<void(*)(void*, int32_t, int32_t, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), a, b, info);
            return 0;
        case 4:
            if (entry.isStatic) return reinterpret_cast<int32_t(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(target)(a, b, c, info);
            return reinterpret_cast<int32_t(*)(void*, int32_t, int32_t, int32_t, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), a, b, c, info);
        case 5:
            if (entry.isStatic) reinterpret_cast<void(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(target)(a, b, c, info);
            else reinterpret_cast<void(*)(void*, int32_t, int32_t, int32_t, const MethodInfo*)>(target)(reinterpret_cast<void*>(self), a, b, c, info);
            return 0;
        default: return 0;
    }
}
constexpr int suspensionSecondsFor(int consecutive) {
    return consecutive < 2 ? 5 : consecutive == 2 ? 10 : 15;
}
static_assert(suspensionSecondsFor(1) == 5 && suspensionSecondsFor(2) == 10 &&
              suspensionSecondsFor(3) == 15 && suspensionSecondsFor(100) == 15);

void applyUniversalHookFault(Entry& entry, const UniversalHookMailbox::Outcome& result) {
    if (!result.faulted) {
        if (result.accepted) {
            entry.timeoutsInWindow = 0;
            entry.timeoutWindow = {};
        }
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (result.retryable && !result.timedOut) {
        counters[9].fetch_add(1, std::memory_order_relaxed);
        entry.retryAfter = now + std::chrono::milliseconds(4);
        return;
    }
    if (!result.timedOut) {
        counters[4].fetch_add(1, std::memory_order_relaxed);
        entry.active.store(false, std::memory_order_release);
        return;
    }
    if (entry.lastTimeout == std::chrono::steady_clock::time_point{} ||
        now - entry.lastTimeout > std::chrono::seconds(60))
        entry.suspensionsInBurst = 0;
    entry.lastTimeout = now;
    if (now - entry.timeoutWindow > std::chrono::seconds(30)) {
        entry.timeoutWindow = now;
        entry.timeoutsInWindow = 0;
    }
    if (++entry.timeoutsInWindow < 3) {
        entry.retryAfter = now + std::chrono::milliseconds(24);
        counters[5].fetch_add(1, std::memory_order_relaxed);
    } else {
        entry.suspended = true;
        entry.lastSuspensionSeconds = suspensionSecondsFor(++entry.suspensionsInBurst);
        entry.retryAfter = now + std::chrono::seconds(entry.lastSuspensionSeconds);
        counters[7].fetch_add(1, std::memory_order_relaxed);
    }
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
                                                       std::chrono::milliseconds(3), 0, false, universalNativeService.load(std::memory_order_acquire));
    if (!result.accepted) counters[2].fetch_add(1, std::memory_order_relaxed);
    else if (!result.faulted) counters[1].fetch_add(1, std::memory_order_relaxed);
    if (result.timedOut) counters[3].fetch_add(1, std::memory_order_relaxed);
    applyUniversalHookFault(entry, result);
    if ((!result.accepted || result.faulted) && !result.originalRan) callOriginal(entry, self, info);
}
int32_t dispatchInt32(int index, uintptr_t self, int32_t value, const MethodInfo* info,
                      int32_t value2 = 0, bool pair = false) {
    auto vanilla = [&](Entry& entry) {
        return pair ? callOriginalInt32Pair(entry, self, value, value2, info)
                    : callOriginalInt32(entry, self, value, info);
    };
    Entry& entry = entries[index];
    if (!entry.active.load(std::memory_order_acquire) || inOriginal || universalNativeAccessInProgress ||
        static_cast<long>(syscall(__NR_gettid)) != gameThread.load(std::memory_order_acquire))
        return vanilla(entry);
    const auto now = std::chrono::steady_clock::now();
    if (now < entry.retryAfter) return vanilla(entry);
    if (entry.suspended) {
        entry.suspended = false;
        entry.timeoutsInWindow = 0;
        entry.timeoutWindow = now;
        counters[8].fetch_add(1, std::memory_order_relaxed);
    }
    int32_t originalValue = 0;
    auto original = [&] {
        inOriginal = true;
        auto body = [&] { originalValue = vanilla(entry); };
        const bool ok = il2cpp::guarded(body);
        inOriginal = false;
        if (ok) universalHookMailbox().setOriginalInt32Value(originalValue);
        return ok;
    };
    counters[0].fetch_add(1, std::memory_order_relaxed);
    const auto result = universalHookMailbox().dispatch(
        entry.managedSlot, self, original, wakeUniversalHookWorker,
        std::chrono::milliseconds(3), value, !pair,
        universalNativeService.load(std::memory_order_acquire), value2, pair);
    if (!result.accepted) counters[2].fetch_add(1, std::memory_order_relaxed);
    else if (!result.faulted) counters[1].fetch_add(1, std::memory_order_relaxed);
    if (result.timedOut) counters[3].fetch_add(1, std::memory_order_relaxed);
    applyUniversalHookFault(entry, result);
    if (!result.accepted || result.faulted)
        return result.originalRan ? originalValue : vanilla(entry);
    return result.value;
}
int32_t dispatchExtended(int index, uintptr_t self, int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    Entry& entry = entries[index];
    auto vanilla = [&] { return callOriginalExtended(entry, self, a, b, c, info); };
    if (!entry.active.load(std::memory_order_acquire) || inOriginal || universalNativeAccessInProgress ||
        static_cast<long>(syscall(__NR_gettid)) != gameThread.load(std::memory_order_acquire))
        return vanilla();
    const auto now = std::chrono::steady_clock::now();
    if (now < entry.retryAfter) return vanilla();
    if (entry.suspended) {
        entry.suspended = false;
        entry.timeoutsInWindow = 0;
        entry.timeoutWindow = now;
        counters[8].fetch_add(1, std::memory_order_relaxed);
    }
    int32_t originalValue = 0;
    auto original = [&] {
        inOriginal = true;
        auto body = [&] { originalValue = vanilla(); };
        const bool ok = il2cpp::guarded(body);
        inOriginal = false;
        if (ok && (entry.extendedMode == 1 || entry.extendedMode == 4))
            universalHookMailbox().setOriginalInt32Value(originalValue);
        return ok;
    };
    counters[0].fetch_add(1, std::memory_order_relaxed);
    const auto result = universalHookMailbox().dispatch(
        entry.managedSlot, self, original, wakeUniversalHookWorker,
        std::chrono::milliseconds(3), a, false,
        universalNativeService.load(std::memory_order_acquire), b, false, c, entry.extendedMode);
    if (!result.accepted) counters[2].fetch_add(1, std::memory_order_relaxed);
    else if (!result.faulted) counters[1].fetch_add(1, std::memory_order_relaxed);
    if (result.timedOut) counters[3].fetch_add(1, std::memory_order_relaxed);
    applyUniversalHookFault(entry, result);
    if (!result.accepted || result.faulted)
        return result.originalRan ? originalValue : vanilla();
    return result.value;
}
template<size_t N> int32_t instanceInt32Stub(void* self, int32_t arg, const MethodInfo* info) {
    return dispatchInt32(N, reinterpret_cast<uintptr_t>(self), arg, info);
}
template<size_t N> int32_t staticInt32Stub(int32_t arg, const MethodInfo* info) {
    return dispatchInt32(N, 0, arg, info);
}
template<size_t N> int32_t instanceInt32PairStub(void* self, int32_t a, int32_t b, const MethodInfo* info) {
    return dispatchInt32(N, reinterpret_cast<uintptr_t>(self), a, info, b, true);
}
template<size_t N> int32_t staticInt32PairStub(int32_t a, int32_t b, const MethodInfo* info) {
    return dispatchInt32(N, 0, a, info, b, true);
}
template<size_t... N> auto instanceInt32PairStubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceInt32PairStub<N>)...};
}
template<size_t... N> auto staticInt32PairStubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticInt32PairStub<N>)...};
}
template<size_t... N> auto instanceInt32Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceInt32Stub<N>)...};
}
template<size_t... N> auto staticInt32Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticInt32Stub<N>)...};
}

template<size_t N> int32_t instanceExtra1(void* self, const MethodInfo* info) {
    return dispatchExtended(N, reinterpret_cast<uintptr_t>(self), 0, 0, 0, info);
}
template<size_t N> int32_t staticExtra1(const MethodInfo* info) {
    return dispatchExtended(N, 0, 0, 0, 0, info);
}
template<size_t... N> auto instanceExtra1Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceExtra1<N>)...};
}
template<size_t... N> auto staticExtra1Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticExtra1<N>)...};
}
template<size_t N> void instanceExtra2(void* self, int32_t a, const MethodInfo* info) {
    dispatchExtended(N, reinterpret_cast<uintptr_t>(self), a, 0, 0, info);
}
template<size_t N> void staticExtra2(int32_t a, const MethodInfo* info) {
    dispatchExtended(N, 0, a, 0, 0, info);
}
template<size_t... N> auto instanceExtra2Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceExtra2<N>)...};
}
template<size_t... N> auto staticExtra2Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticExtra2<N>)...};
}
template<size_t N> void instanceExtra3(void* self, int32_t a, int32_t b, const MethodInfo* info) {
    dispatchExtended(N, reinterpret_cast<uintptr_t>(self), a, b, 0, info);
}
template<size_t N> void staticExtra3(int32_t a, int32_t b, const MethodInfo* info) {
    dispatchExtended(N, 0, a, b, 0, info);
}
template<size_t... N> auto instanceExtra3Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceExtra3<N>)...};
}
template<size_t... N> auto staticExtra3Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticExtra3<N>)...};
}
template<size_t N> int32_t instanceExtra4(void* self, int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    return dispatchExtended(N, reinterpret_cast<uintptr_t>(self), a, b, c, info);
}
template<size_t N> int32_t staticExtra4(int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    return dispatchExtended(N, 0, a, b, c, info);
}
template<size_t... N> auto instanceExtra4Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceExtra4<N>)...};
}
template<size_t... N> auto staticExtra4Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticExtra4<N>)...};
}
template<size_t N> void instanceExtra5(void* self, int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    dispatchExtended(N, reinterpret_cast<uintptr_t>(self), a, b, c, info);
}
template<size_t N> void staticExtra5(int32_t a, int32_t b, int32_t c, const MethodInfo* info) {
    dispatchExtended(N, 0, a, b, c, info);
}
template<size_t... N> auto instanceExtra5Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&instanceExtra5<N>)...};
}
template<size_t... N> auto staticExtra5Stubs(std::index_sequence<N...>) {
    return std::array<void*, sizeof...(N)>{reinterpret_cast<void*>(&staticExtra5<N>)...};
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
    if (install.systemStage)
        return installUniversalSystemStageOnGameThread(install.method, install.slot);
    auto& api = il2cpp::api();
    const size_t dot = install.type.rfind('.');
    if (dot == std::string::npos || install.type.rfind("Terraria.", 0) != 0 ||
        install.method.empty() || install.method.front() == '.' ||
        !accepting.load(std::memory_order_acquire) || install.epoch != epoch.load(std::memory_order_acquire)) return -3;
    if (!api.class_get_methods || !api.method_get_param_count || !api.method_get_name ||
        !api.method_get_return_type || !api.type_get_name || !api.il2cpp_free ||
        !api.class_is_valuetype || !api.method_is_instance || !api.method_is_generic || !api.class_is_generic ||
        ((install.int32Mode || install.int32PairMode || install.extendedMode > 0) && !api.method_get_param)) return -4;
    Il2CppClass* cls = il2cpp::findClassQuiet(install.type.substr(0, dot), install.type.substr(dot + 1));
    if (!cls || api.class_is_valuetype(cls) || api.class_is_generic(cls)) return -5;
    const MethodInfo* selected = nullptr;
    const int expectedArgs = install.extendedMode > 0
        ? (install.extendedMode == 1 ? 0 : install.extendedMode == 2 ? 1 : install.extendedMode == 3 ? 2 : 3)
        : (install.int32PairMode ? 2 : install.int32Mode ? 1 : 0);
    const bool returnsInt32 = install.int32Mode || install.int32PairMode ||
                              install.extendedMode == 1 || install.extendedMode == 4;
    void* iter = nullptr;
    while (const MethodInfo* method = api.class_get_methods(cls, &iter)) {
        if (install.method != api.method_get_name(method) ||
            api.method_get_param_count(method) != static_cast<uint32_t>(expectedArgs) ||
            api.method_is_instance(method) == install.isStatic) continue;
        char* name = api.type_get_name(api.method_get_return_type(method));
        const bool rightReturn = name && std::strcmp(name, returnsInt32 ? "System.Int32" : "System.Void") == 0;
        if (name) api.il2cpp_free(name);
        bool rightParam = true;
        for (int index = 0; index < expectedArgs; ++index) {
            const Il2CppType* param = api.method_get_param(method, static_cast<uint32_t>(index));
            char* paramName = param ? api.type_get_name(param) : nullptr;
            rightParam &= paramName && std::strcmp(paramName, "System.Int32") == 0;
            if (paramName) api.il2cpp_free(paramName);
        }
        if (!rightReturn || !rightParam || selected || api.method_is_generic(method)) return -6;
        selected = method;
    }
    if (!selected || !il2cpp::methodPointer(selected)) return -7;
    for (int i = 0; i < used; ++i) {
        auto& entry = entries[i];
        if (il2cpp::methodPointer(entry.method) != il2cpp::methodPointer(selected)) continue;
        if (entry.active.load(std::memory_order_acquire) || entry.method != selected || entry.isStatic != install.isStatic || entry.int32Mode != install.int32Mode || entry.int32PairMode != install.int32PairMode || entry.extendedMode != install.extendedMode) return -8;
        entry.managedSlot = install.slot;
        entry.retryAfter = {};
        entry.timeoutWindow = {};
        entry.timeoutsInWindow = 0;
        entry.suspensionsInBurst = 0;
        entry.lastSuspensionSeconds = 0;
        entry.lastTimeout = {};
        entry.suspended = false;
        entry.active.store(true, std::memory_order_release);
        return i;
    }
    if (used >= MaxHooks) return -3;
    static const auto instances = instanceStubs(std::make_index_sequence<MaxHooks>{});
    static const auto statics = staticStubs(std::make_index_sequence<MaxHooks>{});
    static const auto intInstances = instanceInt32Stubs(std::make_index_sequence<MaxHooks>{});
    static const auto intStatics = staticInt32Stubs(std::make_index_sequence<MaxHooks>{});
    static const auto pairInstances = instanceInt32PairStubs(std::make_index_sequence<MaxHooks>{});
    static const auto pairStatics = staticInt32PairStubs(std::make_index_sequence<MaxHooks>{});
    static const std::array<std::array<void*, MaxHooks>, 5> extraInstances = {
        instanceExtra1Stubs(std::make_index_sequence<MaxHooks>{}),
        instanceExtra2Stubs(std::make_index_sequence<MaxHooks>{}),
        instanceExtra3Stubs(std::make_index_sequence<MaxHooks>{}),
        instanceExtra4Stubs(std::make_index_sequence<MaxHooks>{}),
        instanceExtra5Stubs(std::make_index_sequence<MaxHooks>{})
    };
    static const std::array<std::array<void*, MaxHooks>, 5> extraStatics = {
        staticExtra1Stubs(std::make_index_sequence<MaxHooks>{}),
        staticExtra2Stubs(std::make_index_sequence<MaxHooks>{}),
        staticExtra3Stubs(std::make_index_sequence<MaxHooks>{}),
        staticExtra4Stubs(std::make_index_sequence<MaxHooks>{}),
        staticExtra5Stubs(std::make_index_sequence<MaxHooks>{})
    };
    Entry& entry = entries[used];
    entry.method = selected;
    entry.isStatic = install.isStatic;
    entry.int32Mode = install.int32Mode;
    entry.int32PairMode = install.int32PairMode;
    entry.extendedMode = install.extendedMode;
    entry.managedSlot = install.slot;
    entry.retryAfter = {};
    entry.timeoutWindow = {};
    entry.timeoutsInWindow = 0;
    entry.suspensionsInBurst = 0;
    entry.lastSuspensionSeconds = 0;
    entry.lastTimeout = {};
    entry.suspended = false;
    void* stub = entry.extendedMode > 0 ? (entry.isStatic ? extraStatics[entry.extendedMode - 1][used] : extraInstances[entry.extendedMode - 1][used])
                 : entry.int32PairMode ? (entry.isStatic ? pairStatics[used] : pairInstances[used])
                 : entry.int32Mode ? (entry.isStatic ? intStatics[used] : intInstances[used])
                 : (entry.isStatic ? statics[used] : instances[used]);
    if (!hook::install(selected, stub, &entry.original)) return -9;
    entry.active.store(true, std::memory_order_release);
    return used++;
#endif
}
}
bool isUniversalHookGameThread() {
    return accepting.load(std::memory_order_acquire) && !inOriginal &&
        !universalNativeAccessInProgress &&
        static_cast<long>(syscall(__NR_gettid)) == gameThread.load(std::memory_order_acquire);
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
    disableAllUniversalSystemStages();
    install.pending = false;
    install.result = -10;
    install.done = true;
    install.wake.notify_one();
}
void enableUniversalHooks() { accepting.store(true, std::memory_order_release); }

int requestInstall(const char* type, const char* method, int slot, int isStatic,
                   bool int32Mode, bool int32PairMode = false, int extendedMode = 0,
                   bool systemStage = false) {
    if (!accepting.load(std::memory_order_acquire) || !type || !method || strnlen(type, 257) > 256 || strnlen(method, 129) > 128 ||
        slot < 0 || slot >= MaxHooks || (isStatic != 0 && isStatic != 1) || extendedMode < 0 || extendedMode > 5) return -1;
    if (!universalCanInstall || !universalLeaveMono || !universalResumeMono) return -12;
    UniversalHookWorkerYield yield;
    std::unique_lock<std::mutex> lock(install.mutex);
    if (install.pending) return -1;
    install.type = type;
    install.method = method;
    install.slot = slot;
    install.isStatic = isStatic != 0;
    install.int32Mode = int32Mode;
    install.int32PairMode = int32PairMode;
    install.extendedMode = extendedMode;
    install.systemStage = systemStage;
    install.epoch = epoch.load(std::memory_order_acquire);
    install.pending = true;
    install.done = false;
    if (!install.wake.wait_for(lock, std::chrono::seconds(2), [] { return install.done; })) {
        install.pending = false;
        return -11;
    }
    return install.result;
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_stage(
    const char* name, int slot) {
    return requestInstall("Terraria.Main", name, slot, 0, false, false, 0, true);
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install(
    const char* type, const char* method, int slot, int isStatic) {
    return requestInstall(type, method, slot, isStatic, false);
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_i32(
    const char* type, const char* method, int slot, int isStatic) {
    return requestInstall(type, method, slot, isStatic, true);
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_i32_pair(
    const char* type, const char* method, int slot, int isStatic) {
    return requestInstall(type, method, slot, isStatic, false, true);
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_extended(
    const char* type, const char* method, int slot, int isStatic, int extendedMode) {
    if (extendedMode < 1 || extendedMode > 5) return -1;
    return requestInstall(type, method, slot, isStatic, false, false, extendedMode);
}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_original_i32(
    int slot, int64_t ticket, int32_t* result) {
    if (!result) return 0;
    int32_t value = 0;
    const bool ok = universalHookMailbox().originalInt32(slot, static_cast<uint64_t>(ticket), value);
    if (ok) *result = value;
    return ok ? 1 : 0;
}
extern "C" __attribute__((visibility("default"))) void bunny_universal_hook_remove(int index) {
    if (index >= 1000 && index < 1010) removeUniversalSystemStage(index);
    else if (index >= 0 && index < MaxHooks) entries[index].active.store(false, std::memory_order_release);
}
extern "C" __attribute__((visibility("default"))) int64_t bunny_universal_hook_stat(int key) {
    if (key == 6) return universalHookLateCompletionCount();
    return key >= 0 && key < 10 ? counters[key].load(std::memory_order_relaxed) : -1;
}
#if defined(BUNNY_UNIVERSAL_HOOK_HOST_TEST)
extern "C" int bunny_universal_hook_test_backoff_seconds(int consecutive) {
    return suspensionSecondsFor(consecutive);
}
extern "C" int bunny_universal_hook_test_last_suspend_seconds(int index) {
    return index >= 0 && index < used ? entries[index].lastSuspensionSeconds : -1;
}
extern "C" int bunny_universal_hook_test_inject_outcome(int index, int kind) {
    if (index < 0 || index >= used || kind < 0 || kind > 3) return -1;
    UniversalHookMailbox::Outcome outcome{};
    outcome.accepted = kind != 3;
    outcome.faulted = kind != 0 && kind != 3;
    outcome.retryable = kind == 1;
    outcome.timedOut = kind == 2;
    applyUniversalHookFault(entries[index], outcome);
    return entries[index].suspended ? 1 : 0;
}
extern "C" int bunny_universal_hook_test_real_streak(int index) {
    return index >= 0 && index < used ? entries[index].timeoutsInWindow : -1;
}
extern "C" int bunny_universal_hook_test_retry_delay_ms(int index) {
    if (index < 0 || index >= used) return -1;
    const auto remaining = entries[index].retryAfter - std::chrono::steady_clock::now();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());
}
extern "C" void bunny_universal_hook_test_expire_suspend(int index) {
    if (index >= 0 && index < used) entries[index].retryAfter = {};
}
#endif
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_original(int slot, int64_t ticket) {
    return universalHookMailbox().original(slot, static_cast<uint64_t>(ticket)) ? 1 : 0;
}
}
