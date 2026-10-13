#include "experiments/csharp/ManagedVisualMonoPump.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

static int calls = 0;
static void* invoke(void*, void*, void**, void** error) {
    *error = nullptr;
    ++calls;
    return calls == 1 ? reinterpret_cast<void*>(1) : nullptr;
}
static char* utf8(void*) {
    const char* value = "V1|2|0|7|0|0|1|0.5|4294967295|5|shield.png";
    auto* result = static_cast<char*>(std::malloc(std::strlen(value) + 1));
    std::strcpy(result, value);
    return result;
}
static void freeString(void* ptr) { std::free(ptr); }

int main() {
    bl::csharp::ManagedVisualQueue queue;
    bl::csharp::ManagedVisualMonoApi api;
    api.invoke = invoke;
    api.utf8 = utf8;
    api.freeString = freeString;
    api.takeWireMethod = reinterpret_cast<void*>(1);
    auto result = bl::csharp::ManagedVisualMonoPump::poll(queue, api);
    assert(result.accepted == 1 && result.exhausted == 1);
    bl::csharp::ManagedVisualCommand command;
    assert(queue.pop(command) && command.effectId == 7);
    assert(queue.size() == 0);
}
