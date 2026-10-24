#include "experiments/csharp/NativeInvasionPersistence.h"
#include "experiments/csharp/NativeWorldSystemPersistence.h"

#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "content/common/GameRefs.h"

#include <cstdint>
#include <string>

namespace bl::csharp {
namespace {

struct PathFields {
    FieldInfo* active = nullptr;
    int path = -1;
    int cloud = -1;
};

PathFields resolvePath() {
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* data = il2cpp::findClass({"Terraria.IO", "FileData", {}});
    if (!main || !data) return {};
    return {il2cpp::findField(main, "ActiveWorldFileData"),
            il2cpp::fieldOffset(data, "_path"),
            il2cpp::fieldOffset(data, "_isCloudSave")};
}

std::string utf8(Il2CppString* value) {
    std::string result;
    if (!value || value->length > 2048) return {};
    for (int i = 0; i < value->length; ++i) {
        uint32_t c = value->chars[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < value->length) {
            const uint32_t d = value->chars[i + 1];
            if (d >= 0xDC00 && d <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
                ++i;
            }
        }
        if (c < 0x80) result.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (c >> 6)));
            result.push_back(static_cast<char>(0x80 | (c & 63)));
        } else if (c < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | (c >> 12)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 63)));
            result.push_back(static_cast<char>(0x80 | (c & 63)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (c >> 18)));
            result.push_back(static_cast<char>(0x80 | ((c >> 12) & 63)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 63)));
            result.push_back(static_cast<char>(0x80 | (c & 63)));
        }
    }
    return result;
}

}

void setWorldPersistenceState(bool ready, uint64_t session,
                              const std::string& path, bool cloud) {
    setNativeInvasionSaveWorld(ready, session, path, cloud);
    setNativeWorldSystemSaveWorld(ready, session, path, cloud);
}

void updateNativeInvasionWorldPath(bool ready, uint64_t session) {
    if (!ready) {
        setWorldPersistenceState(false, session, {}, false);
        return;
    }
    static PathFields fields;
    if (!fields.active || fields.path < 0 || fields.cloud < 0)
        fields = resolvePath();
    if (!fields.active || fields.path < 0 || fields.cloud < 0) {
        setWorldPersistenceState(false, session, {}, false);
        return;
    }
    Il2CppObject* data = nullptr;
    il2cpp::api().field_static_get_value(fields.active, &data);
    if (!data) {
        setWorldPersistenceState(false, session, {}, false);
        return;
    }
    const bool cloud = runtime::field<uint8_t>(data, fields.cloud) != 0;
    auto* value = runtime::field<Il2CppString*>(data, fields.path);
    setWorldPersistenceState(true, session, utf8(value), cloud);
}

}
