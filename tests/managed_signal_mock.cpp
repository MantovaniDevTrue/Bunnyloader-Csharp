#include <atomic>
#include <cassert>
#include <cstdlib>
namespace {
thread_local bool attached = false;
thread_local bool safe = false;
std::atomic<int> entries{0}, exits{0}, initializations{0};
std::atomic<bool> ownerSafe{false};
std::atomic<int> signalChaining{-1}, crashChaining{-1};
}
extern "C" {
void mono_set_signal_chaining(int value) { assert(initializations == 0); signalChaining = value; }
void mono_set_crash_chaining(int value) { assert(initializations == 0); crashChaining = value; }
void* mono_jit_init(const char*) {
    assert(!safe);
#ifdef EXPECT_MONO_NATIVE_SIGNAL_POLICY
    assert(signalChaining == 1 && crashChaining == 0);
#endif
    attached = true;
    ++initializations;
    return reinterpret_cast<void*>(1);
}
void* mono_thread_current() { return attached ? reinterpret_cast<void*>(1) : nullptr; }
void* mono_thread_attach(void*) { assert(!safe); attached = true; return reinterpret_cast<void*>(1); }
#ifndef OMIT_ENTER_GC
void* mono_threads_enter_gc_safe_region(void** marker) {
    assert(marker && attached && !safe);
    safe = true;
    ++entries;
    ownerSafe = true;
    // The bridge must call exit even if the entry cookie is null.
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
void* mono_domain_assembly_open(void*, const char*) { assert(attached && !safe); return reinterpret_cast<void*>(1); }
void* mono_assembly_get_image(void*) { assert(attached && !safe); return reinterpret_cast<void*>(1); }
void* mono_class_from_name(void*, const char*, const char*) { assert(attached && !safe); return reinterpret_cast<void*>(1); }
void* mono_class_get_method_from_name(void*, const char*, int) { assert(attached && !safe); return reinterpret_cast<void*>(1); }
void* mono_runtime_invoke(void*, void*, void** args, void** exception) {
    assert(attached && !safe);
    static int result;
    result = *static_cast<int*>(args[0]) + *static_cast<int*>(args[1]);
    *exception = nullptr;
    return &result;
}
void* mono_object_unbox(void* value) { assert(attached && !safe); return value; }
int test_entries() { return entries; }
int test_exits() { return exits; }
int test_initializations() { return initializations; }
int test_signal_chaining() { return signalChaining; }
int test_crash_chaining() { return crashChaining; }
int test_owner_safe() { return ownerSafe; }
}
