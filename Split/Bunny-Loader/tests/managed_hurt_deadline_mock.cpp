#define mono_class_get_method_from_name original_class_get_method
#define mono_runtime_invoke original_runtime_invoke
#include "managed_pinned_gc_mock.cpp"
#undef mono_class_get_method_from_name
#undef mono_runtime_invoke
#include <thread>

namespace {
std::atomic<bool> blockNext{false}, enteredModify{false}, releaseModify{false};
std::atomic<int> modifications{0};
}

extern "C" {
void* mono_string_new(void*, const char* value) { return const_cast<char*>(value); }
void* mono_class_get_method_from_name(void* klass, const char* name, int parameters) {
    if (!std::strcmp(name, "Register")) return reinterpret_cast<void*>(100);
    if (!std::strcmp(name, "Modify")) return reinterpret_cast<void*>(101);
    if (!std::strcmp(name, "After")) return reinterpret_cast<void*>(102);
    if (!std::strcmp(name, "GetLastError")) return reinterpret_cast<void*>(103);
    return original_class_get_method(klass, name, parameters);
}
void* mono_runtime_invoke(void* method, void* instance, void** args, void** exception) {
    static int registered = 0, after = 1;
    static int64_t modified;
    if (method == reinterpret_cast<void*>(100)) return &registered;
    if (method == reinterpret_cast<void*>(102)) return &after;
    if (method == reinterpret_cast<void*>(103)) return nullptr;
    if (method != reinterpret_cast<void*>(101))
        return original_runtime_invoke(method, instance, args, exception);
    ++modifications;
    if (blockNext.exchange(false)) {
        enteredModify = true;
        while (!releaseModify.load()) std::this_thread::yield();
    }
    modified = *static_cast<int*>(args[1]) / 2 + (int64_t{2} << 20);
    return &modified;
}
void test_block_modify() { enteredModify = false; releaseModify = false; blockNext = true; }
bool test_modify_entered() { return enteredModify.load(); }
void test_release_modify() { releaseModify = true; }
int test_modify_calls() { return modifications.load(); }
}
