#include "content/sounds/NativeSoundRouter.h"

#include "content/sounds/AndroidAudio.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bl::runtime::content {

namespace {

using Clock = std::chrono::steady_clock;
using PlaySoundFn = Il2CppObject* (*)(Il2CppObject*, int32_t, int32_t, int32_t, int32_t,
                                     float, float, const MethodInfo*);

struct ActiveStream {
    int token = 0;
    Clock::time_point end{};
};

struct NativeStyle {
    int soundId = 0;
    float volume = 1.0f;
    float pitch = 0.0f;
    float pitchVariance = 0.0f;
    int maxInstances = 1;
    int limitBehavior = 1;
    int durationMs = 0;
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

enum class AudioCommandKind {
    Play,
    Stop,
};

struct AudioCommand {
    AudioCommandKind kind = AudioCommandKind::Play;
    int token = 0;
    int soundId = 0;
    float left = 1.0f;
    float right = 1.0f;
    float rate = 1.0f;
};

std::mutex g_mutex;
std::vector<NativeStyle> g_styles;
GameAudioRefs g_refs;
PlaySoundFn g_original = nullptr;
bool g_hooked = false;

std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::deque<AudioCommand> g_queue;
std::unordered_map<int, int> g_streams;
std::unordered_set<int> g_cancelled;
std::atomic<int> g_nextToken{kAsyncSoundTokenBase};
std::once_flag g_workerOnce;

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
StaticValueRef<T> resolveStatic(Il2CppClass* cls, const char* fieldName, const char* getterName) {
    StaticValueRef<T> ref;
    if (!cls) return ref;
    ref.field = il2cpp::findField(cls, fieldName);
    if (!ref.field) ref.getter = il2cpp::api().class_get_method_from_name(cls, getterName, 0);
    return ref;
}

template <typename T>
bool resolved(const StaticValueRef<T>& ref) {
    return ref.field || ref.getter;
}

void workerLoop() {
    for (;;) {
        AudioCommand command;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return !g_queue.empty(); });
            command = g_queue.front();
            g_queue.pop_front();
        }

        if (command.kind == AudioCommandKind::Stop) {
            int stream = 0;
            {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_cancelled.insert(command.token);
                const auto it = g_streams.find(command.token);
                if (it != g_streams.end()) {
                    stream = it->second;
                    g_streams.erase(it);
                }
            }
            if (stream > 0) stopSound(stream);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (g_cancelled.erase(command.token) > 0) continue;
        }

        const int stream = playSound(command.soundId, command.left, command.right, command.rate);
        if (stream <= 0) continue;

        bool stopImmediately = false;
        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            if (g_cancelled.erase(command.token) > 0) {
                stopImmediately = true;
            } else {
                g_streams[command.token] = stream;
            }
        }
        if (stopImmediately) stopSound(stream);
    }
}

void ensureWorker() {
    std::call_once(g_workerOnce, [] {
        std::thread(workerLoop).detach();
    });
}

int nextToken() {
    int token = g_nextToken.fetch_add(1, std::memory_order_relaxed);
    if (token < kAsyncSoundTokenBase) {
        g_nextToken.store(kAsyncSoundTokenBase + 1, std::memory_order_relaxed);
        token = kAsyncSoundTokenBase;
    }
    return token;
}

int queuePlay(int soundId, float left, float right, float rate) {
    ensureWorker();
    const int token = nextToken();
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.push_back({AudioCommandKind::Play, token, soundId, left, right, rate});
    }
    g_queueCv.notify_one();
    return token;
}

void queueStop(int token) {
    if (!isSoundStyleToken(token)) return;
    ensureWorker();
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.push_back({AudioCommandKind::Stop, token, 0, 0, 0, 1});
    }
    g_queueCv.notify_one();
}

void prune(NativeStyle& style, Clock::time_point now) {
    style.active.erase(std::remove_if(style.active.begin(), style.active.end(),
        [now](const ActiveStream& stream) { return stream.end <= now; }), style.active.end());
}

void resolveGameAudioRefs() {
    using namespace il2cpp;
    auto& a = api();
    g_refs.main = findClass({"Terraria", "Main", {}});
    if (g_refs.main) {
        if (a.runtime_class_init) a.runtime_class_init(g_refs.main);
        g_refs.screenPosition = resolveStatic<Vec2>(g_refs.main, "screenPosition", "get_screenPosition");
        g_refs.screenWidth = resolveStatic<int32_t>(g_refs.main, "screenWidth", "get_screenWidth");
        g_refs.screenHeight = resolveStatic<int32_t>(g_refs.main, "screenHeight", "get_screenHeight");
        g_refs.soundVolume = resolveStatic<float>(g_refs.main, "soundVolume", "get_soundVolume");
    }

    Il2CppClass* legacy = findClass({"Terraria.Audio", "LegacySoundPlayer", {}});
    if (legacy) {
        if (a.runtime_class_init) a.runtime_class_init(legacy);
        g_refs.attenuation = resolveStatic<float>(legacy, "SoundAttenuationDistance", "get_SoundAttenuationDistance");
    }

    if (!resolved(g_refs.screenPosition)) BL_WARN("sons nativos: Main.screenPosition nao resolvido");
    if (!resolved(g_refs.screenWidth)) BL_WARN("sons nativos: Main.screenWidth nao resolvido");
    if (!resolved(g_refs.screenHeight)) BL_WARN("sons nativos: Main.screenHeight nao resolvido");
    if (!resolved(g_refs.soundVolume)) BL_WARN("sons nativos: Main.soundVolume nao resolvido");
    if (!resolved(g_refs.attenuation)) BL_WARN("sons nativos: SoundAttenuationDistance nao resolvido; usando 2500");
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

    const auto now = Clock::now();
    prune(style, now);
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

    const int duration = style.durationMs > 0 ? style.durationMs : 1000;
    const auto scaled = static_cast<int64_t>(static_cast<double>(duration) / static_cast<double>(rate));
    style.active.push_back({token, now + std::chrono::milliseconds(std::max<int64_t>(1, scaled))});
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

    resolveGameAudioRefs();
    ensureWorker();
    if (!hook::install(method, playSoundHook, &g_original)) {
        BL_ERROR("sons nativos: falha ao instalar roteador");
        return false;
    }
    g_hooked = true;
    return true;
}

int registerSoundStyle(int soundId, float volume, float pitch, float pitchVariance,
                       int maxInstances, int limitBehavior) {
    installNativeSoundRouter();
    std::lock_guard<std::mutex> guard(g_mutex);
    NativeStyle style;
    style.soundId = soundId;
    style.volume = volume;
    style.pitch = pitch;
    style.pitchVariance = pitchVariance;
    style.maxInstances = std::max(0, maxInstances);
    style.limitBehavior = limitBehavior == 0 ? 0 : 1;
    style.durationMs = soundId > 0 ? soundDuration(soundId) : 0;
    g_styles.push_back(std::move(style));
    return static_cast<int>(g_styles.size() - 1);
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
    prune(sound, Clock::now());
    return sound.active.empty() ? 0 : sound.active.back().token;
}

void stopSoundStyleToken(int token) {
    queueStop(token);
}

bool isSoundStyleToken(int token) {
    return token >= kAsyncSoundTokenBase;
}

}
