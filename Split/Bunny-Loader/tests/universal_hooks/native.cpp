#include "experiments/csharp/UniversalHooks.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "il2cpp/Api.h"
#include "il2cpp/GuardedCall.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <vector>

extern "C" int bunny_universal_hook_install(const char*, const char*, int, int);
extern "C" int bunny_universal_hook_install_i32(const char*, const char*, int, int);
extern "C" int bunny_universal_hook_install_i32_pair(const char*, const char*, int, int);
extern "C" int bunny_universal_hook_install_extended(const char*, const char*, int, int, int);
extern "C" int bunny_universal_hook_original_i32(int, int64_t, int32_t*);
extern "C" void bunny_universal_hook_remove(int);
extern "C" int bunny_universal_hook_original(int, int64_t);
extern "C" int64_t bunny_universal_hook_stat(int);
extern "C" void bunny_universal_hook_test_expire_suspend(int);
extern "C" int bunny_universal_hook_test_backoff_seconds(int);
extern "C" int bunny_universal_hook_test_last_suspend_seconds(int);
extern "C" int bunny_universal_hook_test_inject_outcome(int, int);
extern "C" int bunny_universal_hook_test_real_streak(int);
extern "C" int bunny_universal_hook_test_retry_delay_ms(int);
using namespace bl;
using namespace std::chrono_literals;
namespace {
struct Method { void* pointer; const char* name; int count; bool isStatic; bool generic; const char* ret; const char* param = nullptr; const char* param2 = nullptr; const char* param3 = nullptr; };
std::thread::id caller;
int vanilla = 0;
std::atomic<int> stage{0};
void original(void* self, const MethodInfo*) { assert(self == reinterpret_cast<void*>(123)); assert(std::this_thread::get_id() == caller); ++vanilla; stage = 2; }
void originalStatic(const MethodInfo*) { assert(std::this_thread::get_id() == caller); ++vanilla; }
int32_t originalInt32(void* self, int32_t arg, const MethodInfo*) {
    assert(self == reinterpret_cast<void*>(123) && std::this_thread::get_id() == caller);
    ++vanilla;
    return arg + 7;
}
int32_t originalInt32Static(int32_t arg, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller);
    ++vanilla;
    return arg - 2;
}
int32_t originalInt32Pair(void* self, int32_t first, int32_t second, const MethodInfo*) {
    assert(self == reinterpret_cast<void*>(123) && std::this_thread::get_id() == caller);
    ++vanilla;
    return first + second * 2;
}
int32_t originalInt32PairStatic(int32_t first, int32_t second, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller);
    ++vanilla;
    return first - second;
}
int32_t originalExtraZero(void* self, const MethodInfo*) {
    assert(self == reinterpret_cast<void*>(123) && std::this_thread::get_id() == caller);
    ++vanilla;
    return 73;
}
void originalExtraSingle(int32_t a, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller && a == 5);
    ++vanilla;
}
void originalExtraPair(int32_t a, int32_t b, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller && a == 5 && b == 8);
    ++vanilla;
}
int32_t originalExtraTriple(int32_t a, int32_t b, int32_t c, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller && a == 5 && b == 8 && c == 3);
    ++vanilla;
    return a + b + c;
}
void originalExtraVoidTriple(int32_t a, int32_t b, int32_t c, const MethodInfo*) {
    assert(std::this_thread::get_id() == caller && a == 5 && b == 8 && c == 3);
    ++vanilla;
}
void* replacedExtra[5] = {};
void* replaced = nullptr;
void* replacedStatic = nullptr;
void* replacedInt32 = nullptr;
void* replacedInt32Static = nullptr;
void* replacedInt32Pair = nullptr;
void* replacedInt32PairStatic = nullptr;
Method methods[] = {
    {reinterpret_cast<void*>(original), "ResetEffects", 0, false, false, "System.Void"},
    {reinterpret_cast<void*>(originalStatic), "Static", 0, true, false, "System.Void"},
    {reinterpret_cast<void*>(original), "Generic", 0, false, true, "System.Void"},
    {reinterpret_cast<void*>(original), "Update", 1, false, false, "System.Void"},
    {reinterpret_cast<void*>(original), "WrongReturn", 0, false, false, "System.Int32"},
    {reinterpret_cast<void*>(originalInt32), "Int32Transform", 1, false, false, "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32Static), "Int32Static", 1, true, false, "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32), "WrongIntParam", 1, false, false, "System.Int32", "System.Single"},
    {reinterpret_cast<void*>(originalInt32), "WrongIntReturn", 1, false, false, "System.Void", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32Pair), "PairTransform", 2, false, false, "System.Int32", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32PairStatic), "PairStatic", 2, true, false, "System.Int32", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32Pair), "WrongPairSecond", 2, false, false, "System.Int32", "System.Int32", "System.Single"},
    {reinterpret_cast<void*>(originalInt32Pair), "WrongPairReturn", 2, false, false, "System.Void", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalExtraZero), "NextZero", 0, false, false, "System.Int32"},
    {reinterpret_cast<void*>(originalExtraSingle), "FindHome", 1, true, false, "System.Void", "System.Int32"},
    {reinterpret_cast<void*>(originalExtraPair), "PlantCactus", 2, true, false, "System.Void", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalExtraTriple), "countWires", 3, true, false, "System.Int32", "System.Int32", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalExtraVoidTriple), "ConvertTile", 3, true, false, "System.Void", "System.Int32", "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalExtraTriple), "WrongExtraThird", 3, true, false, "System.Int32", "System.Int32", "System.Int32", "System.Single"},
    {reinterpret_cast<void*>(originalExtraPair), "WrongExtraReturn", 2, true, false, "System.Int32", "System.Int32", "System.Int32"}
};
il2cpp::Api api;
int install(const char* method, int slot, bool isStatic = false, bool int32Mode = false, bool pairMode = false) {
    auto result = std::async(std::launch::async, [=] {
        csharp::universalCanInstall = true;
        csharp::universalLeaveMono = +[] {};
        csharp::universalResumeMono = +[] {};
        return pairMode ? bunny_universal_hook_install_i32_pair("Terraria.Player", method, slot, isStatic)
                        : int32Mode ? bunny_universal_hook_install_i32("Terraria.Player", method, slot, isStatic)
                        : bunny_universal_hook_install("Terraria.Player", method, slot, isStatic);
    });
    while (result.wait_for(1ms) != std::future_status::ready) csharp::pumpUniversalHookInstallsOnGameThread();
    return result.get();
}
int installExtended(const char* method, int slot, bool isStatic, int mode) {
    auto result = std::async(std::launch::async, [=] {
        csharp::universalCanInstall = true;
        csharp::universalLeaveMono = +[] {};
        csharp::universalResumeMono = +[] {};
        return bunny_universal_hook_install_extended("Terraria.Player", method, slot, isStatic, mode);
    });
    while (result.wait_for(1ms) != std::future_status::ready)
        csharp::pumpUniversalHookInstallsOnGameThread();
    return result.get();
}
void call() { reinterpret_cast<void(*)(void*, const MethodInfo*)>(replaced)(reinterpret_cast<void*>(123), reinterpret_cast<MethodInfo*>(&methods[0])); }
int32_t callInt32(int32_t value) {
    return reinterpret_cast<int32_t(*)(void*, int32_t, const MethodInfo*)>(replacedInt32)(
        reinterpret_cast<void*>(123), value, reinterpret_cast<MethodInfo*>(&methods[5]));
}
int32_t callInt32Pair(int32_t first, int32_t second) {
    return reinterpret_cast<int32_t(*)(void*, int32_t, int32_t, const MethodInfo*)>(
        replacedInt32Pair)(reinterpret_cast<void*>(123), first, second,
                           reinterpret_cast<MethodInfo*>(&methods[9]));
}
}
namespace bl::il2cpp {
Api& api() { return ::api; }
Il2CppClass* findClassQuiet(const std::string& ns, const std::string& name) {
    return ns == "Terraria" && name == "Player" ? reinterpret_cast<Il2CppClass*>(1) : nullptr;
}
bool guardedCall(void(*body)(void*), void* arg, Caught*) { body(arg); return true; }
}
namespace bl::hook {
bool install(const MethodInfo* method, void* replacement, void** orig) {
    *orig = il2cpp::methodPointer(method);
    if (method == reinterpret_cast<MethodInfo*>(&methods[0])) replaced = replacement;
    else if (method == reinterpret_cast<MethodInfo*>(&methods[6])) replacedInt32Static = replacement;
    else if (method == reinterpret_cast<MethodInfo*>(&methods[5])) replacedInt32 = replacement;
    else if (method == reinterpret_cast<MethodInfo*>(&methods[9])) replacedInt32Pair = replacement;
    else if (method == reinterpret_cast<MethodInfo*>(&methods[10])) replacedInt32PairStatic = replacement;
    else if (method >= reinterpret_cast<MethodInfo*>(&methods[13]) &&
             method <= reinterpret_cast<MethodInfo*>(&methods[17]))
        replacedExtra[(reinterpret_cast<const Method*>(method) - &methods[13])] = replacement;
    else replacedStatic = replacement;
    return true;
}
}
namespace bl::csharp { void wakeUniversalHookWorker() {} }

int main() {
    caller = std::this_thread::get_id();
    api.class_is_valuetype = +[](const Il2CppClass*) { return false; };
    api.class_is_generic = +[](const Il2CppClass*) { return false; };
    api.class_get_methods = +[](Il2CppClass*, void** iter) -> const MethodInfo* {
        size_t index = reinterpret_cast<size_t>(*iter);
        if (index >= std::size(methods)) return nullptr;
        *iter = reinterpret_cast<void*>(index + 1);
        return reinterpret_cast<const MethodInfo*>(&methods[index]);
    };
    api.method_get_name = +[](const MethodInfo* m) { return reinterpret_cast<const Method*>(m)->name; };
    api.method_get_param_count = +[](const MethodInfo* m) -> uint32_t { return reinterpret_cast<const Method*>(m)->count; };
    api.method_is_instance = +[](const MethodInfo* m) { return !reinterpret_cast<const Method*>(m)->isStatic; };
    api.method_is_generic = +[](const MethodInfo* m) { return reinterpret_cast<const Method*>(m)->generic; };
    api.method_get_param = +[](const MethodInfo* m, uint32_t index) -> const Il2CppType* {
        auto* method = reinterpret_cast<const Method*>(m);
        return index == 0 && method->param ? reinterpret_cast<const Il2CppType*>(method->param)
            : index == 1 && method->param2 ? reinterpret_cast<const Il2CppType*>(method->param2)
            : index == 2 && method->param3 ? reinterpret_cast<const Il2CppType*>(method->param3) : nullptr;
    };
    api.method_get_return_type = +[](const MethodInfo* m) { return reinterpret_cast<const Il2CppType*>(reinterpret_cast<const Method*>(m)->ret); };
    api.type_get_name = +[](const Il2CppType* type) { return strdup(reinterpret_cast<const char*>(type)); };
    api.il2cpp_free = free;
    assert(install("Generic", 0) == -6);
    assert(install("Update", 0) == -7);
    assert(install("WrongReturn", 0) == -6);
    assert(install("Missing", 0) == -7);
    int handle = install("ResetEffects", 4);
    assert(handle >= 0 && replaced);
    assert(install("ResetEffects", 5) == -8);
    std::atomic<bool> stop{false};
    std::thread worker([&] {
        while (!stop) {
            csharp::UniversalHookMailbox::Work work{};
            if (!csharp::universalHookMailbox().take(work)) { std::this_thread::yield(); continue; }
            assert(work.slot == 4 && work.self == 123);
            stage = 1;
            assert(bunny_universal_hook_original(work.slot, work.ticket));
            assert(stage == 2);
            stage = 3;
            csharp::universalHookMailbox().complete(work.ticket, true);
        }
    });
    call();
    stop = true; worker.join();
    assert(stage == 3 && vanilla == 1);
    bunny_universal_hook_remove(handle);
    call(); assert(vanilla == 2);
    assert(install("ResetEffects", 5) == handle);
    csharp::disableUniversalHooks();
    call(); assert(vanilla == 3);
    assert(install("Static", 0, true) < 0);
    csharp::enableUniversalHooks();
    int stat = install("Static", 0, true);
    assert(stat >= 0 && replacedStatic);
    bunny_universal_hook_remove(stat);
    reinterpret_cast<void(*)(const MethodInfo*)>(replacedStatic)(reinterpret_cast<MethodInfo*>(&methods[1]));
    assert(vanilla == 4);
    const int64_t timeoutsBefore = bunny_universal_hook_stat(3);
    const int64_t faultsBefore = bunny_universal_hook_stat(4);
    const int64_t recoveriesBefore = bunny_universal_hook_stat(5);
    assert(install("ResetEffects", 5) == handle);
    call();
    assert(vanilla == 5);
    assert(bunny_universal_hook_stat(3) == timeoutsBefore + 1);
    assert(bunny_universal_hook_stat(5) == recoveriesBefore + 1);
    assert(bunny_universal_hook_stat(4) == faultsBefore);
    call();
    assert(vanilla == 6);
    assert(bunny_universal_hook_stat(3) == timeoutsBefore + 1);
    std::this_thread::sleep_for(1050ms);
    call();
    assert(vanilla == 7);
    assert(bunny_universal_hook_stat(5) == recoveriesBefore + 2);
    std::this_thread::sleep_for(1050ms);
    call();
    assert(vanilla == 8);
    assert(bunny_universal_hook_stat(3) == timeoutsBefore + 3);
    assert(bunny_universal_hook_stat(4) == faultsBefore);
    assert(bunny_universal_hook_stat(7) == 1);
    assert(bunny_universal_hook_stat(8) == 0);
    assert(bunny_universal_hook_test_last_suspend_seconds(handle) == 5);
    assert(bunny_universal_hook_test_backoff_seconds(1) == 5);
    assert(bunny_universal_hook_test_backoff_seconds(2) == 10);
    assert(bunny_universal_hook_test_backoff_seconds(3) == 15);
    assert(bunny_universal_hook_test_backoff_seconds(4) == 15);
    call();
    assert(vanilla == 9);
    assert(bunny_universal_hook_stat(3) == timeoutsBefore + 3);
    assert(bunny_universal_hook_stat(0) >= 3);
    const int64_t completedBefore = bunny_universal_hook_stat(1);
    std::atomic<bool> ready{false};
    std::thread resumed([&] {
        ready = true;
        csharp::UniversalHookMailbox::Work work{};
        while (!csharp::universalHookMailbox().take(work)) std::this_thread::yield();
        assert(work.slot == 5 && work.self == 123);
        assert(bunny_universal_hook_original(work.slot, work.ticket));
        assert(csharp::universalHookMailbox().complete(work.ticket, true));
    });
    while (!ready) std::this_thread::yield();
    bunny_universal_hook_test_expire_suspend(handle);
    call();
    resumed.join();
    assert(vanilla == 10);
    assert(bunny_universal_hook_stat(1) == completedBefore + 1);
    assert(bunny_universal_hook_stat(7) == 1);
    assert(bunny_universal_hook_stat(8) == 1);
    assert(bunny_universal_hook_stat(4) == faultsBefore);
    const auto suspensionBaseline = bunny_universal_hook_stat(7);
    for (int i = 0; i < 50; ++i) {
        assert(bunny_universal_hook_test_inject_outcome(handle, 1) == 0);
        assert(bunny_universal_hook_test_real_streak(handle) == 0);
        assert(bunny_universal_hook_test_retry_delay_ms(handle) <= 4);
    }
    assert(bunny_universal_hook_stat(7) == suspensionBaseline);
    assert(bunny_universal_hook_test_inject_outcome(handle, 2) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 1);
    assert(bunny_universal_hook_test_retry_delay_ms(handle) <= 24);
    assert(bunny_universal_hook_test_inject_outcome(handle, 1) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 1);
    assert(bunny_universal_hook_test_inject_outcome(handle, 3) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 1);
    assert(bunny_universal_hook_test_inject_outcome(handle, 0) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 0);
    for (int i = 0; i < 10; ++i) {
        assert(bunny_universal_hook_test_inject_outcome(handle, 2) == 0);
        assert(bunny_universal_hook_test_inject_outcome(handle, 0) == 0);
        assert(bunny_universal_hook_test_real_streak(handle) == 0);
    }
    assert(bunny_universal_hook_stat(7) == suspensionBaseline);
    assert(bunny_universal_hook_test_inject_outcome(handle, 2) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 1);
    assert(bunny_universal_hook_test_inject_outcome(handle, 2) == 0);
    assert(bunny_universal_hook_test_real_streak(handle) == 2);
    assert(bunny_universal_hook_test_inject_outcome(handle, 2) == 1);
    assert(bunny_universal_hook_stat(7) == suspensionBaseline + 1);
    assert(install("Int32Transform", 13, false, false) == -7);
    assert(install("WrongIntParam", 13, false, true) == -6);
    assert(install("WrongIntReturn", 13, false, true) == -6);
    int typedHandle = install("Int32Transform", 13, false, true);
    assert(typedHandle >= 0 && replacedInt32);
    assert(install("Int32Transform", 14, false, true) == -8);
    int baseline = vanilla;
    std::thread typedWorker([&] {
        csharp::UniversalHookMailbox::Work work{};
        while (!csharp::universalHookMailbox().take(work)) std::this_thread::yield();
        assert(work.slot == 13 && work.int32Mode && work.argument == 12);
        int32_t originalResult = -1;
        assert(bunny_universal_hook_original_i32(work.slot, work.ticket, &originalResult));
        assert(originalResult == 19);
        assert(!bunny_universal_hook_original_i32(work.slot, work.ticket, &originalResult));
        assert(csharp::universalHookMailbox().complete(work.ticket, true, originalResult + 30));
    });
    assert(callInt32(12) == 49);
    typedWorker.join();
    assert(vanilla == baseline + 1);
    assert(callInt32(40) == 47);
    assert(vanilla == baseline + 2);
    bunny_universal_hook_remove(typedHandle);
    assert(callInt32(5) == 12);
    int staticHandle = install("Int32Static", 14, true, true);
    assert(staticHandle >= 0 && replacedInt32Static);
    bunny_universal_hook_remove(staticHandle);
    assert(reinterpret_cast<int32_t(*)(int32_t, const MethodInfo*)>(replacedInt32Static)(
        40, reinterpret_cast<MethodInfo*>(&methods[6])) == 38);
    assert(install("PairTransform", 22, false, true) == -7);
    const int firstPairHandle = install("PairTransform", 22, false, false, true);
    assert(firstPairHandle >= 0);
    bunny_universal_hook_remove(firstPairHandle);
    assert(install("WrongPairSecond", 22, false, false, true) == -6);
    assert(install("WrongPairReturn", 22, false, false, true) == -6);
    const int pairHandle = install("PairTransform", 22, false, false, true);
    assert(pairHandle >= 0 && replacedInt32Pair);
    assert(install("PairTransform", 23, false, false, true) == -8);
    int pairBaseline = vanilla;
    std::thread pairWorker([&] {
        csharp::UniversalHookMailbox::Work work{};
        while (!csharp::universalHookMailbox().take(work)) std::this_thread::yield();
        assert(work.slot == 22 && work.int32PairMode && !work.int32Mode);
        assert(work.argument == 4 && work.argument2 == 9 && work.self == 123);
        int32_t originalResult = -1;
        assert(bunny_universal_hook_original_i32(work.slot, work.ticket, &originalResult));
        assert(originalResult == 22);
        assert(!bunny_universal_hook_original_i32(work.slot, work.ticket, &originalResult));
        assert(csharp::universalHookMailbox().complete(work.ticket, true, originalResult + 5));
    });
    assert(callInt32Pair(4, 9) == 27);
    pairWorker.join();
    assert(vanilla == pairBaseline + 1);
    bunny_universal_hook_remove(pairHandle);
    assert(callInt32Pair(4, 9) == 22);
    int pairStaticHandle = install("PairStatic", 24, true, false, true);
    assert(pairStaticHandle >= 0 && replacedInt32PairStatic);
    bunny_universal_hook_remove(pairStaticHandle);
    assert(reinterpret_cast<int32_t(*)(int32_t, int32_t, const MethodInfo*)>(replacedInt32PairStatic)(
        12, 5, reinterpret_cast<MethodInfo*>(&methods[10])) == 7);

    assert(installExtended("WrongExtraThird", 30, true, 4) == -6);
    assert(installExtended("WrongExtraReturn", 30, true, 3) == -6);
    assert(installExtended("NextZero", 30, false, 2) == -7);
    for (int mode = 1; mode <= 5; ++mode) {
        static const char* names[] = {"NextZero", "FindHome", "PlantCactus", "countWires", "ConvertTile"};
        int handle = installExtended(names[mode-1], 30 + mode, mode != 1, mode);
        assert(handle >= 0 && replacedExtra[mode-1]);
        const int before = vanilla;
        std::thread worker([&] {
            csharp::UniversalHookMailbox::Work work{};
            while (!csharp::universalHookMailbox().take(work)) std::this_thread::yield();
            assert(work.extendedMode == mode && work.slot == 30 + mode);
            assert(work.self == (mode == 1 ? 123u : 0u));
            if (mode != 1) assert(work.argument == 5);
            if (mode == 3 || mode >= 4) assert(work.argument2 == 8);
            if (mode >= 4) assert(work.argument3 == 3);
            int32_t value = 0;
            if (mode == 1 || mode == 4) {
                assert(bunny_universal_hook_original_i32(work.slot, work.ticket, &value));
                assert(value == (mode == 1 ? 73 : 16));
                assert(!bunny_universal_hook_original_i32(work.slot, work.ticket, &value));
            } else {
                assert(bunny_universal_hook_original(work.slot, work.ticket));
                assert(!bunny_universal_hook_original(work.slot, work.ticket));
            }
            assert(csharp::universalHookMailbox().complete(work.ticket, true,
                mode == 1 || mode == 4 ? value + 11 : 0));
        });
        const MethodInfo* info = reinterpret_cast<MethodInfo*>(&methods[12 + mode]);
        int value = 0;
        switch (mode) {
            case 1: value = reinterpret_cast<int32_t(*)(void*, const MethodInfo*)>(replacedExtra[0])(reinterpret_cast<void*>(123), info); break;
            case 2: reinterpret_cast<void(*)(int32_t, const MethodInfo*)>(replacedExtra[1])(5, info); break;
            case 3: reinterpret_cast<void(*)(int32_t, int32_t, const MethodInfo*)>(replacedExtra[2])(5, 8, info); break;
            case 4: value = reinterpret_cast<int32_t(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(replacedExtra[3])(5, 8, 3, info); break;
            case 5: reinterpret_cast<void(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(replacedExtra[4])(5, 8, 3, info); break;
        }
        worker.join();
        assert(vanilla == before + 1);
        if (mode == 1) assert(value == 84);
        if (mode == 4) assert(value == 27);
        bunny_universal_hook_remove(handle);
        if (mode == 1) assert(reinterpret_cast<int32_t(*)(void*, const MethodInfo*)>(replacedExtra[0])(reinterpret_cast<void*>(123), info) == 73);
        if (mode == 2) reinterpret_cast<void(*)(int32_t, const MethodInfo*)>(replacedExtra[1])(5, info);
        if (mode == 3) reinterpret_cast<void(*)(int32_t, int32_t, const MethodInfo*)>(replacedExtra[2])(5, 8, info);
        if (mode == 4) assert(reinterpret_cast<int32_t(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(replacedExtra[3])(5, 8, 3, info) == 16);
        if (mode == 5) reinterpret_cast<void(*)(int32_t, int32_t, int32_t, const MethodInfo*)>(replacedExtra[4])(5, 8, 3, info);
        assert(vanilla == before + 2);
    }
    std::thread retryableWorker([&] {
        csharp::UniversalHookMailbox::Work work{};
        while (!csharp::universalHookMailbox().take(work)) std::this_thread::yield();
        assert(work.slot == 21 && work.self == 123);
        assert(csharp::universalHookMailbox().complete(work.ticket, false, 0, true));
    });
    const auto transient = csharp::universalHookMailbox().dispatch(
        21, 123, [] { return true; }, [] {}, std::chrono::milliseconds(500));
    retryableWorker.join();
    assert(transient.accepted && transient.faulted && transient.retryable && !transient.timedOut);
    assert(!csharp::universalHookCallbackIsTerminal(transient.accepted, false, transient.retryable));
    assert(csharp::universalHookCallbackIsTerminal(true, false));
    assert(!csharp::universalHookCallbackIsTerminal(false, false));
    std::cout << "native: typed int/void 0..3 argument ABI, metadata, original and return override PASS\n";
    std::cout << "native: metadata, original affinity, adaptive 5/10/15s suspension, auto-rearm, protected original PASS\n";
}
