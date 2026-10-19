#include "content/sounds/NativeSoundRouter.h"

#include "content/sounds/AndroidAudio.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/FieldContract.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace bl::runtime::content {

namespace {

using Clock = std::chrono::steady_clock;
using PlaySoundFn = Il2CppObject* (*)(Il2CppObject*, int32_t, int32_t, int32_t, int32_t,
                                     float, float, const MethodInfo*);

struct ActiveStream {
    int token = 0;
};

struct NativeStyle {
    bool released = false;
    int soundId = 0;
    float volume = 1.0f;
    float pitch = 0.0f;
    float pitchVariance = 0.0f;
    int maxInstances = 1;
    int limitBehavior = 1;
    std::vector<ActiveStream> active;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

template <typename T>
struct StaticValueRef {
    FieldInfo* field = nullptr;
    const MethodInfo* getter = nullptr;
};

struct GameAudioRefs {
    Il2CppClass* main = nullptr;
    StaticValueRef<Vec2> screenPosition;
    StaticValueRef<int32_t> screenWidth;
    StaticValueRef<int32_t> screenHeight;
    StaticValueRef<float> soundVolume;
    StaticValueRef<float> attenuation;
};

std::mutex g_mutex;
std::vector<NativeStyle> g_styles;
GameAudioRefs g_refs;
PlaySoundFn g_original = nullptr;
bool g_hooked = false;

float clampf(float value, float min, float max) {
    return value < min ? min : value > max ? max : value;
}

float randomCentered() {
    thread_local uint32_t state = static_cast<uint32_t>(Clock::now().time_since_epoch().count()) | 1u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0x00FFFFFFu) / 16777216.0f - 0.5f;
}

template <typename T>
T staticValue(const StaticValueRef<T>& ref, T fallback) {
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

template <typename T>
StaticValueRef<T> resolveStatic(Il2CppClass* cls, const char* fieldName, const char* getterName,
                                const char* expected) {
    StaticValueRef<T> ref;
    auto& a=il2cpp::api(); std::string error;
    if(!cls) return ref;
    FieldInfo* field=nullptr;
    if(il2cpp::nativeFieldContract(cls,fieldName,expected,true,sizeof(T),&field,nullptr,&error)) {
        ref.field=field; return ref;
    }
    const auto fail=[&](const char* actual) {
        BL_WARN("[NativeSound][WARN] Field=%s Getter=%s Stage=BindAudioRefs Expected=Static:%s Actual=%s Detail=%s Reason=AudioReferenceContractMismatch",fieldName,getterName,expected,actual,error.c_str());
        return ref;
    };
    if(!a.class_get_method_from_name||!a.method_is_instance||!a.method_get_param_count||
       !a.method_get_return_type||!a.class_from_il2cpp_type||!a.class_value_size||!a.runtime_invoke||
       !a.class_get_namespace||!a.class_get_name||!a.class_is_valuetype)
        return fail("RequiredExportMissing");
    const auto* getter=a.class_get_method_from_name(cls,getterName,0);
    if(!getter||a.method_is_instance(getter)||a.method_get_param_count(getter)!=0) return fail("MissingOrNonStaticZeroParameterGetter");
    auto* type=a.class_from_il2cpp_type(a.method_get_return_type(getter));
    const char* ns=type?a.class_get_namespace(type):nullptr;
    const char* name=type?a.class_get_name(type):nullptr;
    uint32_t alignment=0;
    if(!ns||!name||std::string(ns)+"."+name!=expected||!a.class_is_valuetype(type)||
       a.class_value_size(type,&alignment)!=static_cast<int>(sizeof(T))) return fail("GetterReturnTypeOrSizeMismatch");
    ref.getter=getter; return ref;
}

template <typename T>
bool resolved(const StaticValueRef<T>& ref) {
    return ref.field || ref.getter;
}

// The shared Android SfxQueue bounds pending work/history and owns stream cleanup.
// Keep the legacy token range so JavaScript stop/findActive calls remain compatible.

int queuePlay(int soundId, float left, float right, float rate) {
    const int handle = enqueueSound(-soundId, soundId, left, right, rate, 0, false);
    if (handle<=0) return 0;
    if (handle>=kAsyncSoundTokenBase) {cancelSound(handle);return 0;}
    return kAsyncSoundTokenBase+handle;
}

void queueStop(int token) {
    if (token>kAsyncSoundTokenBase) cancelSound(token-kAsyncSoundTokenBase);
}

void prune(NativeStyle& style) {
    style.active.erase(std::remove_if(style.active.begin(), style.active.end(),
        [](const ActiveStream& stream) {
            const int state=playbackState(stream.token-kAsyncSoundTokenBase);
            return state<=0 || state>=4; // Pending, playing and paused remain tracked.
        }), style.active.end());
}

bool resolveGameAudioRefs() {
    using namespace il2cpp;
    auto& a = api();
    g_refs = {};
    g_refs.main = findClass({"Terraria", "Main", {}});
    if (g_refs.main) {
        if (a.runtime_class_init) a.runtime_class_init(g_refs.main);
        g_refs.screenPosition = resolveStatic<Vec2>(g_refs.main, "screenPosition", "get_screenPosition", "Microsoft.Xna.Framework.Vector2");
        g_refs.screenWidth = resolveStatic<int32_t>(g_refs.main, "screenWidth", "get_screenWidth", "System.Int32");
        g_refs.screenHeight = resolveStatic<int32_t>(g_refs.main, "screenHeight", "get_screenHeight", "System.Int32");
        g_refs.soundVolume = resolveStatic<float>(g_refs.main, "soundVolume", "get_soundVolume", "System.Single");
    }

    Il2CppClass* legacy = findClass({"Terraria.Audio", "LegacySoundPlayer", {}});
    if (legacy) {
        if (a.runtime_class_init) a.runtime_class_init(legacy);
        g_refs.attenuation = resolveStatic<float>(legacy, "SoundAttenuationDistance", "get_SoundAttenuationDistance", "System.Single");
    }

    if (!resolved(g_refs.screenPosition)) BL_WARN("sons nativos: Main.screenPosition nao resolvido");
    if (!resolved(g_refs.screenWidth)) BL_WARN("sons nativos: Main.screenWidth nao resolvido");
    if (!resolved(g_refs.screenHeight)) BL_WARN("sons nativos: Main.screenHeight nao resolvido");
    if (!resolved(g_refs.soundVolume)) BL_WARN("sons nativos: Main.soundVolume nao resolvido");
    if (!resolved(g_refs.attenuation)) BL_WARN("sons nativos: SoundAttenuationDistance nao resolvido; usando 2500");
    return resolved(g_refs.screenPosition)&&resolved(g_refs.screenWidth)&&resolved(g_refs.screenHeight)&&resolved(g_refs.soundVolume);
}

int playStyleLocked(NativeStyle& style, int x, int y, float volumeScale, float pitchOffset) {
    if (style.soundId <= 0) return 0;

    float volume = 1.0f;
    float pan = 0.0f;
    if (x != -1 && y != -1) {
        const float attenuation = staticValue<float>(g_refs.attenuation, 2500.0f);
        const Vec2 screen = staticValue<Vec2>(g_refs.screenPosition, {});
        const int width = staticValue<int32_t>(g_refs.screenWidth, 1920);
        const int height = staticValue<int32_t>(g_refs.screenHeight, 1080);
        const float halfWidth = std::max(1.0f, static_cast<float>(width) * 0.5f);
        const float cx = screen.x + halfWidth;
        const float cy = screen.y + static_cast<float>(height) * 0.5f;
        const float dx = static_cast<float>(x) - cx;
        const float dy = static_cast<float>(y) - cy;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (!(attenuation > 0.0f) || distance >= attenuation) return 0;
        pan = clampf(dx / halfWidth, -1.0f, 1.0f);
        volume = 1.0f - distance / attenuation;
    }

    volume *= style.volume * volumeScale * staticValue<float>(g_refs.soundVolume, 1.0f);
    if (!(volume > 0.0f)) return 0;

    prune(style);
    if (style.maxInstances > 0 && static_cast<int>(style.active.size()) >= style.maxInstances) {
        if (style.limitBehavior == 0) return 0;
        queueStop(style.active.front().token);
        style.active.erase(style.active.begin());
    }

    float rate = std::pow(2.0f, style.pitch + randomCentered() * style.pitchVariance + pitchOffset);
    rate = clampf(rate, 0.5f, 2.0f);
    const float left = clampf(volume * std::min(1.0f, 1.0f - pan), 0.0f, 1.0f);
    const float right = clampf(volume * std::min(1.0f, 1.0f + pan), 0.0f, 1.0f);
    const int token = queuePlay(style.soundId, left, right, rate);
    if(token<=0) return 0;

    style.active.push_back({token});
    return token;
}

Il2CppObject* playSoundHook(Il2CppObject* self, int32_t type, int32_t x, int32_t y, int32_t style,
                            float volumeScale, float pitchOffset, const MethodInfo* method) {
    if (type == kCustomSoundMarkerId) {
        playSoundStyle(style, x, y, volumeScale, pitchOffset);
        return nullptr;
    }

    const auto original = __atomic_load_n(&g_original, __ATOMIC_ACQUIRE);
    return original ? original(self, type, x, y, style, volumeScale, pitchOffset, method) : nullptr;
}

}

bool installNativeSoundRouter() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_hooked) return true;

    using namespace il2cpp;
    Il2CppClass* cls = findClass({"Terraria.Audio", "LegacySoundPlayer", {}});
    const MethodInfo* method = cls ? findMethodBySignature(cls, parseSignature(
        "SoundEffectInstance PlaySound(int type, int x, int y, int Style, float volumeScale, float pitchOffset)")) : nullptr;
    if (!method) {
        BL_ERROR("sons nativos: LegacySoundPlayer.PlaySound nao encontrado");
        return false;
    }

    if(!api().field_static_get_value||!api().method_is_instance||!api().method_is_instance(method)||!resolveGameAudioRefs()) {
        BL_ERROR("[NativeSound][ERROR] Stage=BindRouter Expected=InstancePlaySoundAndTypedStaticAudioRefs Actual=NativeContractMismatch Result=RouterRejected");
        return false;
    }
    if (!hook::install(method, playSoundHook, &g_original)) {
        BL_ERROR("sons nativos: falha ao instalar roteador");
        return false;
    }
    g_hooked = true;
    return true;
}

int registerSoundStyle(int soundId, float volume, float pitch, float pitchVariance,
                       int maxInstances, int limitBehavior) {
    if (!installNativeSoundRouter()) return -1;
    std::lock_guard<std::mutex> guard(g_mutex);
    NativeStyle style;
    style.soundId = soundId;
    style.volume = volume;
    style.pitch = pitch;
    style.pitchVariance = pitchVariance;
    style.maxInstances = std::max(0, maxInstances);
    style.limitBehavior = limitBehavior == 0 ? 0 : 1;
    for (size_t i=0;i<g_styles.size();++i) {
        if (g_styles[i].released) {g_styles[i]=std::move(style);return static_cast<int>(i);}
    }
    g_styles.push_back(std::move(style));
    return static_cast<int>(g_styles.size() - 1);
}

void releaseSoundStyle(int style) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (style<0 || static_cast<size_t>(style)>=g_styles.size()) return;
    for (const auto& stream:g_styles[static_cast<size_t>(style)].active) queueStop(stream.token);
    g_styles[static_cast<size_t>(style)]=NativeStyle{};
    g_styles[static_cast<size_t>(style)].released=true;
}

int playSoundStyle(int style, int x, int y, float volumeScale, float pitchOffset) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (style < 0 || static_cast<size_t>(style) >= g_styles.size()) return 0;
    return playStyleLocked(g_styles[static_cast<size_t>(style)], x, y, volumeScale, pitchOffset);
}

int findActiveSoundStyle(int style) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (style < 0 || static_cast<size_t>(style) >= g_styles.size()) return 0;
    NativeStyle& sound = g_styles[static_cast<size_t>(style)];
    prune(sound);
    return sound.active.empty() ? 0 : sound.active.back().token;
}

void stopSoundStyleToken(int token) {
    queueStop(token);
}

bool isSoundStyleToken(int token) {
    return token >= kAsyncSoundTokenBase;
}

}
