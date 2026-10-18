#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
namespace {
thread_local bool attached = false;
thread_local bool safe = false;
std::atomic<int> entries{0}, exits{0}, initializations{0};
std::atomic<bool> ownerSafe{false};
std::atomic<int> hits{0}, hitsV2{0}, hitsV3{0}, hitsV4{0}, kills{0};
std::atomic<bool> gameplayEffects{false};
std::atomic<bool> impulseEffects{false};
}
extern "C" {
void* mono_jit_init(const char*) {
    assert(!safe);
    attached = true;
    safe = true;
    ownerSafe = true;
    ++initializations;
    return reinterpret_cast<void*>(1);
}
void* mono_thread_current() { return attached ? reinterpret_cast<void*>(1) : nullptr; }
void* mono_thread_attach(void*) { assert(attached && safe); return reinterpret_cast<void*>(1); }
#ifndef OMIT_ENTER_GC
void* mono_threads_enter_gc_safe_region(void** marker) {
    assert(marker && attached && !safe);
    safe = true;
    ++entries;
    ownerSafe = true;
    return nullptr;
}
#endif
#ifndef OMIT_EXIT_GC
void mono_threads_exit_gc_safe_region(void* cookie, void** marker) {
    assert(!cookie && marker && attached && safe);
    ownerSafe = false;
    safe = false;
    ++exits;
}
#endif
void* mono_domain_assembly_open(void*, const char*) { assert(attached && safe); return reinterpret_cast<void*>(1); }
void* mono_assembly_get_image(void*) { assert(attached && safe); return reinterpret_cast<void*>(1); }
void* mono_class_from_name(void*, const char*, const char*) { assert(attached && safe); return reinterpret_cast<void*>(1); }
void* mono_class_get_method_from_name(void*, const char* name, int params) {
    assert(attached && safe);
    if (!std::strcmp(name, "OnHitNPC")) {
        assert(params == 2);
        return reinterpret_cast<void*>(2);
    }
    if (!std::strcmp(name, "OnHitNPCV2")) {
        assert(params == 3);
        return reinterpret_cast<void*>(4);
    }
    if (!std::strcmp(name, "OnHitNPCV3")) {
        assert(params == 7);
        return reinterpret_cast<void*>(5);
    }
    if (!std::strcmp(name, "OnHitNPCV4")) {
        assert(params == 12);
        return reinterpret_cast<void*>(6);
    }
    if (!std::strcmp(name, "OnKill")) {
        assert(params == 2);
        return reinterpret_cast<void*>(3);
    }
    if (!std::strcmp(name, "GetHookSnapshot")) {
        assert(params == 0);
        return reinterpret_cast<void*>(7);
    }
    assert(params == 2);
    return reinterpret_cast<void*>(1);
}
void* mono_runtime_invoke(void* method, void*, void** args, void** exception) {
    assert(attached && safe);
    static int result;
    if (method == reinterpret_cast<void*>(7)) {
        *exception = nullptr;
        return reinterpret_cast<void*>(8);
    }
    const int tick = *static_cast<int*>(args[0]);
    const int damage = *static_cast<int*>(args[1]);
    if (method == reinterpret_cast<void*>(2))
        result = damage + tick % 7 + ++hits;
    else if (method == reinterpret_cast<void*>(4))
        result = damage + tick % 7 + *static_cast<int*>(args[2]) + ++hitsV2;
    else if (method == reinterpret_cast<void*>(5))
        result = 5000 + damage + tick % 7 +
            *static_cast<int*>(args[2]) + *static_cast<int*>(args[3]) +
            *static_cast<int*>(args[4]) + static_cast<int>(*static_cast<float*>(args[5])) +
            static_cast<int>(*static_cast<float*>(args[6])) + ++hitsV3;
    else if (method == reinterpret_cast<void*>(6)) {
        const int* npcIndex = static_cast<int*>(args[2]);
        const int* npcType = static_cast<int*>(args[3]);
        const int* npcLife = static_cast<int*>(args[4]);
        const int* playerIndex = static_cast<int*>(args[7]);
        const int* playerLife = static_cast<int*>(args[8]);
        const int* playerLifeMax = static_cast<int*>(args[9]);
        const float* npcX = static_cast<float*>(args[5]);
        const float* npcY = static_cast<float*>(args[6]);
        const float* playerX = static_cast<float*>(args[10]);
        const float* playerY = static_cast<float*>(args[11]);
        assert(*playerLife > 0 && *playerLifeMax >= *playerLife);
        result = impulseEffects.load(std::memory_order_relaxed) ? 2000235 :
            gameplayEffects.load(std::memory_order_relaxed) ? 1044180 :
            10000 + damage + tick % 7 + *npcIndex + *npcType + *npcLife +
            *playerIndex + *playerLife + *playerLifeMax +
            static_cast<int>(*npcX) + static_cast<int>(*npcY) +
            static_cast<int>(*playerX) + static_cast<int>(*playerY) + ++hitsV4;
    }
    else if (method == reinterpret_cast<void*>(3))
        result = damage + tick % 5 + ++kills;
    else
        result = tick + damage;
    *exception = nullptr;
    return &result;
}
void* mono_object_unbox(void* value) { assert(attached && safe); return value; }
int test_entries() { return entries; }
int test_exits() { return exits; }
int test_initializations() { return initializations; }
int test_owner_safe() { return ownerSafe; }
int test_legacy_hits() { return hits; }
int test_v2_hits() { return hitsV2; }
int test_v3_hits() { return hitsV3; }
int test_v4_hits() { return hitsV4; }
void test_enable_effect_mode() { gameplayEffects.store(true); }
void test_enable_impulse_mode() { impulseEffects.store(true); }
int test_kills() { return kills; }
}
