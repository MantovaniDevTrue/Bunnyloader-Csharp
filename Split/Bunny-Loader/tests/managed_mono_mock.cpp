#include <cstring>
#include <cstdlib>

extern "C" {

void* mono_jit_init(const char*) {
    if (std::getenv("BUNNY_MONO_SIMULATE_ABORT")) std::_Exit(86);
    return reinterpret_cast<void*>(1);
}
void* mono_thread_attach(void*) { return reinterpret_cast<void*>(1); }
void* mono_domain_assembly_open(void*, const char*) { return reinterpret_cast<void*>(1); }
void* mono_assembly_get_image(void*) { return reinterpret_cast<void*>(1); }
void* mono_class_from_name(void*, const char* space, const char* name) {
    if (std::strcmp(space, "BunnyLoader.ManagedProbe") != 0 ||
        std::strcmp(name, "Probe") != 0) return nullptr;
    return reinterpret_cast<void*>(1);
}
void* mono_class_get_method_from_name(void*, const char* name, int count) {
    if (count != 2) return nullptr;
    if (std::strcmp(name, "Run") == 0) return reinterpret_cast<void*>(1);
    if (std::strcmp(name, "AI") == 0) return reinterpret_cast<void*>(2);
    return nullptr;
}
void* mono_runtime_invoke(void* method, void*, void** args, void** exception) {
    static int result;
    *exception = nullptr;
    const int a = *static_cast<int*>(args[0]);
    const int b = *static_cast<int*>(args[1]);
    if (method == reinterpret_cast<void*>(2)) result = b + (a % 7) - 3;
    else result = a + b;
    return &result;
}
void* mono_object_unbox(void* value) { return value; }

}
