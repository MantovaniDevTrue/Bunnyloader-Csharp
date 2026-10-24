#pragma once
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <cstring>

namespace bl::csharp {
// Mobile Main.gameMenu forwards to the current LocalUser context. It is
// a static property, not a static field on Main.
inline const MethodInfo* resolveMobileMainMenu() {
    auto* main = il2cpp::findClassQuiet("Terraria", "Main");
    const auto* method = main ? il2cpp::findMethodBySignature(
        main, il2cpp::parseSignature("bool get_gameMenu()")) : nullptr;
    auto& api = il2cpp::api();
    return method && api.method_is_instance && !api.method_is_instance(method)
        ? method : nullptr;
}
inline bool readMobileMainMenu(const MethodInfo* getter, bool* inMenu) {
    auto& api = il2cpp::api();
    if (!getter || !inMenu || !api.runtime_invoke) return false;
    Il2CppObject* exception = nullptr;
    auto* boxed = api.runtime_invoke(getter, nullptr, nullptr, &exception);
    if (exception || !boxed) return false;
    uint8_t value = 1;
    std::memcpy(&value, reinterpret_cast<const char*>(boxed) + sizeof(Il2CppObject), 1);
    if (value > 1) return false;
    *inMenu = value != 0;
    return true;
}
} // namespace bl::csharp
