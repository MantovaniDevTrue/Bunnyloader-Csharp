#include "experiments/csharp/NativeAchievementLocale.h"

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <string>

namespace bl::csharp {
namespace {
std::string toUtf8(const Il2CppString* source) {
    if (!source || source->length < 1 || source->length > 64) return {};
    std::string value;
    for (int i = 0; i < source->length; ++i) {
        std::uint32_t c = source->chars[i];
        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 1 >= source->length) return {};
            std::uint32_t second = source->chars[++i];
            if (second < 0xDC00 || second > 0xDFFF) return {};
            c = 0x10000 + ((c - 0xD800) << 10) + second - 0xDC00;
        } else if (c >= 0xDC00 && c <= 0xDFFF) return {};
        if (c < 0x80) value.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            value.push_back(static_cast<char>(0xC0 | (c >> 6)));
            value.push_back(static_cast<char>(0x80 | (c & 63)));
        } else if (c < 0x10000) {
            value.push_back(static_cast<char>(0xE0 | (c >> 12)));
            value.push_back(static_cast<char>(0x80 | ((c >> 6) & 63)));
            value.push_back(static_cast<char>(0x80 | (c & 63)));
        } else {
            value.push_back(static_cast<char>(0xF0 | (c >> 18)));
            value.push_back(static_cast<char>(0x80 | ((c >> 12) & 63)));
            value.push_back(static_cast<char>(0x80 | (c & 63)));
        }
    }
    return value;
}
}

bool currentNativeAchievementCulture(std::string* culture, std::string* error) {
    if (!culture || !error) return false;
    culture->clear();
    error->clear();
    const auto& a = il2cpp::api();
    if (!a.runtime_invoke || !a.method_is_instance || !a.method_get_return_type ||
        !a.class_from_il2cpp_type || !a.class_get_name) {
        *error = "Localization IL2CPP API unavailable";
        return false;
    }
    auto* language = il2cpp::findClass({
        "Terraria.Localization", "Language", {}});
    auto* gameCulture = il2cpp::findClass({
        "Terraria.Localization", "GameCulture", {}});
    if (!language || !gameCulture) {
        *error = "Language/GameCulture unavailable";
        return false;
    }
    static const MethodInfo* getCulture = nullptr;
    static const MethodInfo* getName = nullptr;
    if (!getCulture) {
        getCulture = il2cpp::findMethodBySignature(language,
            il2cpp::parseSignature("GameCulture get_ActiveCulture()"));
        if (!getCulture || a.method_is_instance(getCulture) ||
            a.class_from_il2cpp_type(a.method_get_return_type(getCulture)) != gameCulture) {
            *error = "Language.ActiveCulture signature unavailable";
            return false;
        }
    }
    if (!getName) {
        getName = il2cpp::findMethodBySignature(gameCulture,
            il2cpp::parseSignature("string get_Name()"));
        if (!getName || !a.method_is_instance(getName)) {
            *error = "GameCulture.Name signature unavailable";
            return false;
        }
    }
    Il2CppObject* exception = nullptr;
    Il2CppObject* active = a.runtime_invoke(getCulture, nullptr, nullptr, &exception);
    if (exception || !active) {
        *error = "Active culture not ready";
        return false;
    }
    Il2CppObject* raw = a.runtime_invoke(getName, active, nullptr, &exception);
    if (exception || !raw) {
        *error = "GameCulture.Name read failed";
        return false;
    }
    *culture = toUtf8(reinterpret_cast<Il2CppString*>(raw));
    if (culture->empty() || culture->size() > 32) {
        *error = "Culture name empty or invalid";
        return false;
    }
    for (char ch : *culture)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-')) {
            culture->clear();
            *error = "Unsafe culture name rejected";
            return false;
        }
    return true;
}

}
