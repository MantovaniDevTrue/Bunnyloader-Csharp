#define mono_runtime_invoke mock_runtime_invoke
#include "managed_pinned_gc_mock.cpp"
#undef mono_runtime_invoke
namespace { std::atomic<bool> failNext{false}; }
extern "C" void test_fail_next() { failNext = true; }
extern "C" void* mono_runtime_invoke(void* method, void* obj, void** args, void** exception) {
    if (failNext.exchange(false)) {
        *exception = reinterpret_cast<void*>(1);
        return nullptr;
    }
    return mock_runtime_invoke(method, obj, args, exception);
}
