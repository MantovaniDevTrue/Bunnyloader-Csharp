#define mono_class_get_method_from_name original_class_get_method
#define mono_runtime_invoke original_runtime_invoke
#include "../../managed_pinned_gc_mock.cpp"
#undef mono_class_get_method_from_name
#undef mono_runtime_invoke
#include <thread>
#include <string>
#include <set>
#include <mutex>

namespace {
std::atomic<bool> blockNext{false}, entered{false}, released{false};
std::atomic<int> loads{0}, unloads{0}, shutdowns{0}, active{0}, wrongThread{0}, afterLate{0};
std::thread::id owner;
std::set<std::string> loaded;
std::string diagnostic;
bool lateCleaned = false;
}
extern "C" {
void* mono_string_new(void*, const char* value) { return const_cast<char*>(value); }
void* mono_class_get_method_from_name(void* klass, const char* name, int parameters) {
    if (!std::strcmp(name, "Dispatch")) { assert(parameters == 3); return reinterpret_cast<void*>(201); }
    if (!std::strcmp(name, "get_LastError")) { assert(parameters == 0); return reinterpret_cast<void*>(202); }
    return original_class_get_method(klass, name, parameters);
}
char* mono_string_to_utf8(void* value) { return ::strdup(static_cast<const char*>(value)); }
void mono_free(void* value) { std::free(value); }
void* mono_runtime_invoke(void* method, void* instance, void** args, void** exception) {
    if (method == reinterpret_cast<void*>(202)) { *exception = nullptr; return diagnostic.data(); }
    if (method != reinterpret_cast<void*>(201)) return original_runtime_invoke(method, instance, args, exception);
    assert(attached && safe);
    if (owner == std::thread::id{}) owner = std::this_thread::get_id();
    if (owner != std::this_thread::get_id()) ++wrongThread;
    const int op = *static_cast<int*>(args[0]);
    std::string id = static_cast<const char*>(args[1]);
    static int result;
    *exception = nullptr;
    result = 0;
    if (op == 0) {
        ++loads;
        if (blockNext.exchange(false)) {
            entered = true;
            while (!released.load()) std::this_thread::yield();
        }
        if (id == "fail") { diagnostic = "simulated managed registration failure"; result = -1; return &result; }
        if (id == "after-late") { assert(lateCleaned); ++afterLate; }
        loaded.insert(id); active = static_cast<int>(loaded.size());
        result = 2;
    } else if (op == 1) {
        ++unloads;
        loaded.erase(id); active = static_cast<int>(loaded.size());
        if (id == "late") lateCleaned = true;
    } else {
        ++shutdowns;
        loaded.clear(); active = 0;
    }
    return &result;
}
void test_block_content() { entered = false; released = false; blockNext = true; }
void test_release_content() { released = true; }
int test_content_entered() { return entered.load(); }
int test_content_loads() { return loads.load(); }
int test_content_unloads() { return unloads.load(); }
int test_content_shutdowns() { return shutdowns.load(); }
int test_content_active() { return active.load(); }
int test_content_wrong_thread() { return wrongThread.load(); }
int test_content_after_late() { return afterLate.load(); }
}
