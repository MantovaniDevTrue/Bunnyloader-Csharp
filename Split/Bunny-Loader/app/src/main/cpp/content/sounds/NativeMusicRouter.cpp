#include "content/sounds/NativeMusicRouter.h"

#include "content/sounds/AndroidAudio.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace bl::runtime::content {

namespace {

using UpdateAudioFn = void (*)(Il2CppObject*, const MethodInfo*);

template <typename T>
struct StaticRef {
    FieldInfo* field = nullptr;
    const MethodInfo* getter = nullptr;
};

struct MusicRefs {
    StaticRef<bool> gameMenu;
    StaticRef<int32_t> curMusic;
    StaticRef<float> musicVolume;
    FieldInfo* musicFade = nullptr;
    const MethodInfo* musicFadeGetter = nullptr;
};

struct Track {
    int id = 0;
    float fade = 0.0f;
    int direction = 0;
};

constexpr float kFade = 0.005f;
constexpr float kAudible = 0.25f;
constexpr float kFramesPerSecond = 60.0f;

std::mutex g_mutex;
std::vector<Track> g_tracks;
MusicRefs g_refs;
UpdateAudioFn g_original = nullptr;
std::atomic<int> g_target{0};
float g_masterVolume = -1.0f;
bool g_hooked = false;

template <typename T>
StaticRef<T> resolveStatic(Il2CppClass* cls, const char* fieldName, const char* getterName) {
    StaticRef<T> ref;
    if (!cls) return ref;
    ref.field = il2cpp::findField(cls, fieldName);
    if (!ref.field) ref.getter = il2cpp::api().class_get_method_from_name(cls, getterName, 0);
    return ref;
}

template <typename T>
T readStatic(const StaticRef<T>& ref, T fallback) {
    auto& a = il2cpp::api();
    if (ref.field) {
        T value = fallback;
        a.field_static_get_value(ref.field, &value);
        return value;
    }
    if (!ref.getter || !a.runtime_invoke) return fallback;
    Il2CppObject* exception = nullptr;
    Il2CppObject* boxed = a.runtime_invoke(ref.getter, nullptr, nullptr, &exception);
    if (!boxed || exception) return fallback;
    T value = fallback;
    std::memcpy(&value, reinterpret_cast<const char*>(boxed) + sizeof(Il2CppObject), sizeof(T));
    return value;
}

Il2CppArray* readFadeArray() {
    auto& a = il2cpp::api();
    if (g_refs.musicFade) {
        Il2CppArray* value = nullptr;
        a.field_static_get_value(g_refs.musicFade, &value);
        return value;
    }
    if (!g_refs.musicFadeGetter || !a.runtime_invoke) return nullptr;
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = a.runtime_invoke(g_refs.musicFadeGetter, nullptr, nullptr, &exception);
    return exception ? nullptr : reinterpret_cast<Il2CppArray*>(value);
}

float vanillaFade(int index) {
    Il2CppArray* array = readFadeArray();
    if (!array || index < 0 || static_cast<uintptr_t>(index) >= array->length) return 0.0f;
    return static_cast<float*>(arrayData(array))[index];
}

Track* findTrack(int id) {
    for (auto& track : g_tracks) {
        if (track.id == id) return &track;
    }
    return nullptr;
}

int rampDurationMs(float fromFade, float toFade) {
    const float distance = std::fabs(toFade - fromFade);
    const float frames = distance / kFade;
    return std::max(1, static_cast<int>(std::lround(frames * 1000.0f / kFramesPerSecond)));
}

void resolveRefs() {
    using namespace il2cpp;
    auto& a = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    if (!main) return;
    if (a.runtime_class_init) a.runtime_class_init(main);
    g_refs.gameMenu = resolveStatic<bool>(main, "gameMenu", "get_gameMenu");
    g_refs.curMusic = resolveStatic<int32_t>(main, "curMusic", "get_curMusic");
    g_refs.musicVolume = resolveStatic<float>(main, "musicVolume", "get_musicVolume");
    g_refs.musicFade = findField(main, "musicFade");
    if (!g_refs.musicFade) g_refs.musicFadeGetter = a.class_get_method_from_name(main, "get_musicFade", 0);
}

void dispatchRamp(Track& track, int direction, float masterVolume) {
    const float from = track.fade * masterVolume;
    if (direction > 0) {
        rampMusicVolume(track.id, from, masterVolume, rampDurationMs(track.fade, 1.0f));
    } else if (direction < 0) {
        rampMusicVolume(track.id, from, 0.0f, rampDurationMs(track.fade, 0.0f));
    } else {
        rampMusicVolume(track.id, from, from, 1);
    }
    track.direction = direction;
}

void step() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_tracks.empty()) return;

    const int targetId = readStatic<bool>(g_refs.gameMenu, false)
        ? 0
        : g_target.load(std::memory_order_relaxed);
    Track* target = targetId > 0 ? findTrack(targetId) : nullptr;

    if (target) target->fade = std::min(1.0f, target->fade + kFade);

    bool mainAudible = false;
    if (target) {
        mainAudible = target->fade > kAudible;
    } else {
        const int current = readStatic<int32_t>(g_refs.curMusic, 0);
        mainAudible = current <= 0 || vanillaFade(current) > kAudible;
    }

    const float volume = std::clamp(readStatic<float>(g_refs.musicVolume, 1.0f), 0.0f, 1.0f);
    const bool masterChanged = g_masterVolume < 0.0f || std::fabs(volume - g_masterVolume) > 0.0001f;
    g_masterVolume = volume;

    for (auto& track : g_tracks) {
        const bool isTarget = &track == target;
        if (!isTarget && mainAudible) track.fade = std::max(0.0f, track.fade - kFade);

        int direction = 0;
        if (isTarget && track.fade < 1.0f) direction = 1;
        else if (!isTarget && mainAudible && track.fade > 0.0f) direction = -1;

        if (direction != track.direction || masterChanged) {
            dispatchRamp(track, direction, volume);
        }
    }
}

void updateAudioHook(Il2CppObject* self, const MethodInfo* method) {
    const auto original = __atomic_load_n(&g_original, __ATOMIC_ACQUIRE);
    if (original) original(self, method);
    step();
}

}

bool installNativeMusicRouter() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_hooked) return true;

    using namespace il2cpp;
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* method = main
        ? findMethodBySignature(main, parseSignature("void UpdateAudio()"))
        : nullptr;
    if (!method) {
        BL_ERROR("musica nativa: Main.UpdateAudio nao encontrado");
        return false;
    }

    resolveRefs();
    if (!hook::install(method, updateAudioHook, &g_original)) {
        BL_ERROR("musica nativa: falha ao instalar roteador");
        return false;
    }

    g_hooked = true;
    BL_INFO("musica nativa: fade assincrono instalado; Main.UpdateAudio nao entra no QuickJS");
    return true;
}

void registerNativeMusicTrack(int id) {
    if (id <= 0) return;
    installNativeMusicRouter();
    std::lock_guard<std::mutex> guard(g_mutex);
    if (findTrack(id)) return;
    g_tracks.push_back({id, 0.0f, 0});
}

void setNativeMusicTarget(int id) {
    g_target.store(id > 0 ? id : 0, std::memory_order_relaxed);
}

float nativeMusicFade(int id) {
    std::lock_guard<std::mutex> guard(g_mutex);
    Track* track = findTrack(id);
    return track ? track->fade : 0.0f;
}

}
