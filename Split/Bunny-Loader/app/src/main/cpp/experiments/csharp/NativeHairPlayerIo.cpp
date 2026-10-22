#include "experiments/csharp/NativeHairPlayerIo.h"

#include "experiments/csharp/NativeHairSaveState.h"
#include "experiments/csharp/NativeHairSaveSession.h"
#include "experiments/csharp/NativeHairSidecar.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace bl::csharp::hair {
namespace {

using namespace bl::il2cpp;
using SaveFn = void (*)(Il2CppObject*, const MethodInfo*);
using LoadFn = Il2CppObject* (*)(Il2CppString*, bool, const MethodInfo*);

struct Refs {
    Il2CppClass* playerClass = nullptr;
    Il2CppClass* fileClass = nullptr;
    int32_t hair = -1;
    int32_t filePlayer = -1;
    const MethodInfo* playerGetter = nullptr;
    const MethodInfo* pathGetter = nullptr;
    const MethodInfo* cloudGetter = nullptr;
    const MethodInfo* saveMethod = nullptr;
    const MethodInfo* loadMethod = nullptr;
    bool saveHooked = false;
    bool loadHooked = false;
};

Refs refs;
SaveFn originalSave = nullptr;
LoadFn originalLoad = nullptr;
const Registry* activeRegistry = nullptr;
PlayerHairSaveSession saveSession;
bool active = false;
HairPlayerIoStatus state;

bool integerField(Il2CppClass* cls, const char* name, int32_t* offset) {
    auto& a = api();
    if (!cls || !a.class_from_il2cpp_type || !a.field_get_type ||
        !a.class_get_namespace || !a.class_get_name ||
        !a.class_instance_size || !a.field_get_flags) return false;
    FieldInfo* field = findField(cls, name);
    if (!field || (a.field_get_flags(field) & 0x10u)) return false;
    auto* type = a.class_from_il2cpp_type(a.field_get_type(field));
    if (!type || std::strcmp(a.class_get_namespace(type), "System") ||
        std::strcmp(a.class_get_name(type), "Int32")) return false;
    const int32_t place = fieldOffset(cls, name);
    const uint32_t size = a.class_instance_size(cls);
    if (place < static_cast<int32_t>(sizeof(Il2CppObject)) || place < 0 ||
        static_cast<uint32_t>(place) > size ||
        sizeof(int32_t) > size - static_cast<uint32_t>(place)) return false;
    *offset = place;
    return true;
}

std::string utf8(const Il2CppString* value) {
    if (!value || value->length < 0 || value->length > 3072) return {};
    const std::u16string_view text(value->chars, static_cast<size_t>(value->length));
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t code = text[i];
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (++i >= text.size()) return {};
            const uint32_t low = text[i];
            if (low < 0xDC00 || low > 0xDFFF) return {};
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        } else if (code >= 0xDC00 && code <= 0xDFFF) return {};
        if (code <= 0x7F) out.push_back(static_cast<char>(code));
        else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    return out;
}

Il2CppObject* invokeObject(const MethodInfo* method, Il2CppObject* self) {
    if (!method || !self) return nullptr;
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = api().runtime_invoke(method, self, nullptr, &exception);
    return exception ? nullptr : value;
}

Il2CppObject* filePlayer(Il2CppObject* file) {
    if (!file) return nullptr;
    Il2CppObject* player = invokeObject(refs.playerGetter, file);
    if (!refs.playerGetter && refs.filePlayer >= 0) {
        std::memcpy(&player, reinterpret_cast<const char*>(file) + refs.filePlayer,
                    sizeof(player));
    }
    return player && api().object_get_class(player) == refs.playerClass ? player : nullptr;
}

bool isLocalFile(Il2CppObject* file) {
    Il2CppObject* boxed = invokeObject(refs.cloudGetter, file);
    return boxed && *(reinterpret_cast<uint8_t*>(boxed) + sizeof(Il2CppObject)) == 0;
}

std::string filePath(Il2CppObject* file) {
    auto* result = reinterpret_cast<Il2CppString*>(invokeObject(refs.pathGetter, file));
    return utf8(result);
}

int* hairField(Il2CppObject* player) {
    return player ? reinterpret_cast<int*>(reinterpret_cast<char*>(player) + refs.hair) : nullptr;
}

void save(Il2CppObject* file, const MethodInfo* method) {
    if (!originalSave) return;
    if (!active || !activeRegistry || !file || !isLocalFile(file)) {
        originalSave(file, method);
        return;
    }
    Il2CppObject* player = filePlayer(file);
    int* hair = hairField(player);
    const std::string path = filePath(file);
    if (!hair || NativeHairSidecarPath(path).empty()) {
        originalSave(file, method);
        return;
    }

    const int selected = *hair;
    SaveDecision decision = saveSession.BeforeSave(path, selected, *activeRegistry);
    {
        VanillaSaveGuard vanilla(hair, *activeRegistry);
        originalSave(file, method);
        // RAII reestabelece o cabelo antes de interagir com os dados da sessão.
    }

    std::string error;
    bool completed = false;
    if (decision.action == SidecarAction::Update)
        completed = WriteNativeHairSidecar(path, decision.stableKey, &error);
    else if (decision.action == SidecarAction::Remove)
        completed = RemoveNativeHairSidecar(path, &error);
    else
        completed = true;
    if (completed) saveSession.Commit(path, decision, selected);
    // Uma falha na escrita mantém o tracker original para não apagar dados
    // de mods ausentes. O hook nativo só deve ser ativado após QA Android.
}

Il2CppObject* load(Il2CppString* fileName, bool cloud,
                   const MethodInfo* method) {
    Il2CppObject* data = originalLoad ? originalLoad(fileName, cloud, method) : nullptr;
    if (!active || !activeRegistry || cloud || !data) return data;

    const std::string path = utf8(fileName);
    Il2CppObject* player = filePlayer(data);
    int* hair = hairField(player);
    if (!hair || NativeHairSidecarPath(path).empty()) return data;

    std::string key, error;
    if (ReadNativeHairSidecar(path, &key, &error) != SidecarRead::Found) return data;
    int type = 0;
    if (!activeRegistry->RestoreKey(key, &type)) return data;
    *hair = type;
    saveSession.OnLoaded(path, key, type);
    return data;
}

bool bind(std::string* error) {
    auto& a = api();
    if (!a.method_is_instance || !a.runtime_invoke || !a.object_get_class ||
        !a.field_get_type || !a.class_from_il2cpp_type ||
        !a.class_get_method_from_name) {
        *error = "HairPlayerIl2CppApiUnavailable";
        return false;
    }

    refs.playerClass = findClass({"Terraria", "Player", {}});
    refs.fileClass = findClass({"Terraria.IO", "PlayerFileData", {}});
    if (!refs.playerClass || !refs.fileClass ||
        !integerField(refs.playerClass, "hair", &refs.hair)) {
        *error = "PlayerFileDataOrHairFieldUnavailable";
        return false;
    }
    refs.playerGetter = findMethodBySignature(refs.fileClass, parseSignature("Player get_Player()"));
    if (!refs.playerGetter) {
        FieldInfo* direct = findField(refs.fileClass, "Player");
        if (direct && a.field_get_flags && !(a.field_get_flags(direct) & 0x10u)) {
            refs.filePlayer = fieldOffset(refs.fileClass, "Player");
            const auto size = a.class_instance_size(refs.fileClass);
            if (refs.filePlayer < static_cast<int32_t>(sizeof(Il2CppObject)) ||
                static_cast<uint32_t>(refs.filePlayer) > size ||
                sizeof(void*) > size - static_cast<uint32_t>(refs.filePlayer))
                refs.filePlayer = -1;
        }
    } else if (!a.method_is_instance(refs.playerGetter)) refs.playerGetter = nullptr;

    refs.pathGetter = findMethodBySignature(refs.fileClass, parseSignature("string get_Path()"));
    refs.cloudGetter = findMethodBySignature(refs.fileClass, parseSignature("bool get_IsCloudSave()"));
    refs.saveMethod = findMethodBySignature(
        refs.playerClass, parseSignature("void InternalSavePlayerFile(PlayerFileData playerFile)"));
    refs.loadMethod = findMethodBySignature(
        refs.playerClass, parseSignature("PlayerFileData LoadPlayer(string playerPath, bool cloudSave)"));

    if ((!refs.playerGetter && refs.filePlayer < 0) ||
        !refs.pathGetter || !refs.cloudGetter ||
        !a.method_is_instance(refs.pathGetter) || !a.method_is_instance(refs.cloudGetter) ||
        !refs.saveMethod || !refs.loadMethod ||
        a.method_is_instance(refs.saveMethod) || a.method_is_instance(refs.loadMethod)) {
        *error = "PlayerSaveLoadMetadataUnavailable";
        return false;
    }
    return true;
}

} // namespace

HairPlayerIoStatus InstallNativeHairPlayerIo(const Registry* registry,
                                             bool texturesInstalled) {
    if (!registry || registry->Size() == 0 || !texturesInstalled)
        return {refs.saveHooked && refs.loadHooked, false, "HairNotReady"};
    std::string error;
    if (!bind(&error)) return {false, false, error};
    if (!refs.saveHooked) {
        if (!bl::hook::install(refs.saveMethod, save, &originalSave))
            return {false, false, "HairPlayerSaveHookFailed"};
        refs.saveHooked = true;
    }
    if (!refs.loadHooked) {
        if (!bl::hook::install(refs.loadMethod, load, &originalLoad))
            return {refs.saveHooked, false, "HairPlayerLoadHookFailed"};
        refs.loadHooked = true;
    }
    saveSession.Reset();
    activeRegistry = registry;
    active = true;
    state = {true, true, {}};
    return state;
}

void ResetNativeHairPlayerIo() {
    active = false;
    activeRegistry = nullptr;
    saveSession.Reset();
    state.enabled = false;
}

} // namespace bl::csharp::hair
