#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
namespace {
thread_local bool attached = false;
thread_local bool safe = false;
std::atomic<int> entries{0}, exits{0}, initializations{0};
std::atomic<bool> ownerSafe{false};
std::atomic<int> hits{0}, kills{0};
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
    assert(attached && safe && params == 2);
    if (!std::strcmp(name, "OnHitNPC")) return reinterpret_cast<void*>(2);
    if (!std::strcmp(name, "OnKill")) return reinterpret_cast<void*>(3);
    return reinterpret_cast<void*>(1);
}
void* mono_runtime_invoke(void* method, void*, void** args, void** exception) {
    assert(attached && safe);
    static int result;
    const int tick = *static_cast<int*>(args[0]);
    const int damage = *static_cast<int*>(args[1]);
    if (method == reinterpret_cast<void*>(2))
        result = damage + tick % 7 + ++hits;
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
}
