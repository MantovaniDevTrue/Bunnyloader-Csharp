// Exercise the production Mobile property reader and network Color ABI.
#include "il2cpp/Api.h"
struct Il2CppClass {};
struct MethodInfo {};
#include "experiments/csharp/NativeCommandChatHooks.cpp"
#include <cassert>
#include <array>
#include <iostream>
namespace {
bl::il2cpp::Api fixture;
MethodInfo menuGetter, colorCtor, literal, send;
Il2CppClass colorType;
struct ColorBox { Il2CppObject header; uint32_t rgba; } colorBox;
struct BoolBox { Il2CppObject header; uint8_t value; } boolBox;
Il2CppObject textObject, networkObject, errorObject;
bool fail = false;
int sends = 0;
}
namespace bl::il2cpp { Api& api() { return fixture; } }
int main() {
    using namespace bl::csharp;
    fixture.runtime_invoke = [](const MethodInfo* method, void* self, void** args, Il2CppObject** error) -> Il2CppObject* {
        *error = fail ? &errorObject : nullptr;
        if (fail) return nullptr;
        if (method == &menuGetter) { assert(!self && !args); return &boolBox.header; }
        if (method == &literal) { assert(!self && args[0] == &textObject); return &networkObject; }
        if (method == &colorCtor) {
            assert(self == &colorBox.rgba); // value-type this excludes the boxed header
            assert(*static_cast<int*>(args[0]) == 17);
            assert(*static_cast<int*>(args[1]) == 128);
            assert(*static_cast<int*>(args[2]) == 240);
            assert(*static_cast<int*>(args[3]) == 255);
            colorBox.rgba = 0xfff08011;
            return nullptr;
        }
        assert(method == &send && !self);
        assert(args[0] == &networkObject);
        assert(*static_cast<uint32_t*>(args[1]) == 0xfff08011);
        assert(*static_cast<int*>(args[2]) == 7);
        ++sends;
        return nullptr;
    };
    bool menu = true;
    assert(!readMobileMainMenu(nullptr, &menu) && menu);
    boolBox.value = 0; assert(readMobileMainMenu(&menuGetter, &menu) && !menu);
    boolBox.value = 1; assert(readMobileMainMenu(&menuGetter, &menu) && menu);
    fail = true; menu = true;
    assert(!readMobileMainMenu(&menuGetter, &menu) && menu);
    fail = false;
    fixture.string_new = [](const char*) { return reinterpret_cast<Il2CppString*>(&textObject); };
    fixture.object_new = [](Il2CppClass* type) { assert(type == &colorType); return &colorBox.header; };
    g_networkLiteral = &literal; g_sendClient = &send;
    g_colorConstructor = &colorCtor; g_colorClass = &colorType;
    assert(writeNetwork("QA", 7, 17, 128, 240) && sends == 1);
    assert(!writeNetwork("QA", 255, 17, 128, 240) && sends == 1);
    fail = true;
    assert(!writeNetwork("QA", 7, 17, 128, 240) && sends == 1);
    std::cout << "Mobile menu property and chat Color ABI PASS\n";
}
