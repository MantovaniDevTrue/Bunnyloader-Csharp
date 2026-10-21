#include "experiments/csharp/NativeAchievementProfile.h"

#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp {
namespace {

std::string utf8Path(const Il2CppString* text) {
    if (!text || text->length < 1 || text->length > 2048) return {};
    std::string out;
    out.reserve(static_cast<std::size_t>(text->length));
    for (int i = 0; i < text->length; ++i) {
        uint32_t codepoint = text->chars[i];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (i + 1 >= text->length) return {};
            const uint32_t next = text->chars[++i];
            if (next < 0xDC00 || next > 0xDFFF) return {};
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) +
                        (next - 0xDC00);
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) return {};
        if (codepoint == 0) return {};
        if (codepoint < 0x80) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 63)));
        } else if (codepoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 63)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 63)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 63)));
        }
        if (out.size() > 2048) return {};
    }
    return out;
}

}

bool activeAchievementProfile(std::string* profileKey, std::string* error) {
    if (!profileKey || !error) return false;
    profileKey->clear();
    error->clear();
    auto& api = il2cpp::api();
    if (!api.runtime_invoke || !api.field_get_type ||
        !api.class_from_il2cpp_type || !api.class_get_name ||
        !api.class_get_namespace) {
        *error = "Player profile IL2CPP exports unavailable";
        return false;
    }
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* playerData = il2cpp::findClass({
        "Terraria.IO", "PlayerFileData", {}});
    Il2CppClass* fileData = il2cpp::findClass({
        "Terraria.IO", "FileData", {}});
    if (!main || !playerData || !fileData) {
        *error = "PlayerFileData types unavailable";
        return false;
    }
    const MethodInfo* getter = api.class_get_method_from_name(
        main, "get_ActivePlayerFileData", 0);
    const auto* returnType = getter && api.method_get_return_type
        ? api.method_get_return_type(getter) : nullptr;
    Il2CppClass* resultClass = returnType
        ? api.class_from_il2cpp_type(returnType) : nullptr;
    if (!getter || resultClass != playerData) {
        *error = "ActivePlayerFileData getter signature changed";
        return false;
    }
    int pathOffset = -1, cloudOffset = -1;
    if (!il2cpp::nativeFieldContract(fileData, "_path", "System.String",
            false, sizeof(void*), nullptr, &pathOffset, error) ||
        !il2cpp::nativeFieldContract(fileData, "_isCloudSave", "System.Boolean",
            false, sizeof(std::uint8_t), nullptr, &cloudOffset, error))
        return false;

    Il2CppObject* exception = nullptr;
    Il2CppObject* data = api.runtime_invoke(getter, nullptr, nullptr, &exception);
    if (exception || !data) {
        *error = "Active player profile not available";
        return false;
    }
    const auto* base = reinterpret_cast<const char*>(data);
    Il2CppString* path = nullptr;
    std::uint8_t cloud = 0;
    std::memcpy(&path, base + pathOffset, sizeof(path));
    std::memcpy(&cloud, base + cloudOffset, sizeof(cloud));
    const std::string utf8 = utf8Path(path);
    if (utf8.empty()) {
        *error = "Player path unavailable for profile isolation";
        return false;
    }
    *profileKey = achievementProfileKey(utf8, cloud != 0);
    if (profileKey->empty()) {
        *error = "Player profile hash rejected";
        return false;
    }
    return true;
}

}
