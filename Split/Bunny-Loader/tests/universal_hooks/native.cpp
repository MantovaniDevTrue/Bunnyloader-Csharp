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
extern "C" int bunny_universal_hook_original_i32(int, int64_t, int32_t*);
extern "C" void bunny_universal_hook_remove(int);
extern "C" int bunny_universal_hook_original(int, int64_t);
extern "C" int64_t bunny_universal_hook_stat(int);
extern "C" void bunny_universal_hook_test_expire_suspend(int);
using namespace bl;
using namespace std::chrono_literals;
namespace {
struct Method { void* pointer; const char* name; int count; bool isStatic; bool generic; const char* ret; const char* param = nullptr; };
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
void* replaced = nullptr;
void* replacedStatic = nullptr;
void* replacedInt32 = nullptr;
void* replacedInt32Static = nullptr;
Method methods[] = {
    {reinterpret_cast<void*>(original), "ResetEffects", 0, false, false, "System.Void"},
    {reinterpret_cast<void*>(originalStatic), "Static", 0, true, false, "System.Void"},
    {reinterpret_cast<void*>(original), "Generic", 0, false, true, "System.Void"},
    {reinterpret_cast<void*>(original), "Update", 1, false, false, "System.Void"},
    {reinterpret_cast<void*>(original), "WrongReturn", 0, false, false, "System.Int32"},
    {reinterpret_cast<void*>(originalInt32), "Int32Transform", 1, false, false, "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32Static), "Int32Static", 1, true, false, "System.Int32", "System.Int32"},
    {reinterpret_cast<void*>(originalInt32), "WrongIntParam", 1, false, false, "System.Int32", "System.Single"},
    {reinterpret_cast<void*>(originalInt32), "WrongIntReturn", 1, false, false, "System.Void", "System.Int32"}
};
il2cpp::Api api;
int install(const char* method, int slot, bool isStatic = false) {
    auto result = std::async(std::launch::async, [=] {
        csharp::universalCanInstall = true;
        csharp::universalLeaveMono = +[] {};
        csharp::universalResumeMono = +[] {};
        return bunny_universal_hook_install("Terraria.Player", method, slot, isStatic);
    });
    while (result.wait_for(1ms) != std::future_status::ready) csharp::pumpUniversalHookInstallsOnGameThread();
    return result.get();
}
void call() { reinterpret_cast<void(*)(void*, const MethodInfo*)>(replaced)(reinterpret_cast<void*>(123), reinterpret_cast<MethodInfo*>(&methods[0])); }
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
    std::cout << "native: metadata, original affinity, cooldown, 30s circuit suspension, auto-rearm, protected original PASS\n";
}
