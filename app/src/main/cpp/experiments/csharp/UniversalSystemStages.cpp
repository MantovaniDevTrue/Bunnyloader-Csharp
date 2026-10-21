#include "experiments/csharp/UniversalSystemStages.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "experiments/csharp/UniversalHooks.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace bl::csharp {
#if defined(__aarch64__)
namespace {
using Clock = std::chrono::steady_clock;
constexpr int kCount = 9;
struct alignas(16) RegisterFrame {
    uint64_t general[31];
    uint64_t flags;
    alignas(16) uint8_t vectors[32][16];
};
static_assert(sizeof(RegisterFrame) == 768);

struct Stage {
    const char* name;
    size_t offset;
    uint32_t expected[4];
    void (*bridge)();
    void** original;
    std::atomic<int> slot{-1};
    std::atomic<bool> active{false};
    std::atomic<int64_t> retryAfterNs{0};
    std::atomic<int> faults{0};
};
extern Stage stages[kCount];
using WorldUpdateFn = void (*)(void*, const MethodInfo*);
WorldUpdateFn originalWorldUpdate = nullptr;
std::atomic<bool> rootInstalled{false};

extern "C" {
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal0 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal1 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal2 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal3 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal4 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal5 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal6 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal7 = nullptr;
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal8 = nullptr;
}

void invokeStage(int index) {
    Stage& stage = stages[index];
    if (!stage.active.load(std::memory_order_acquire)) return;
    const int slot = stage.slot.load(std::memory_order_acquire);
    if (slot < 0 || !isUniversalHookGameThread()) return;
    const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count();
    if (now < stage.retryAfterNs.load(std::memory_order_relaxed)) return;

    // A rotina original deste estágio é o próprio salto ARM64 para o corpo
    // seguinte de DoUpdateInWorld. Nunca executá-la via MonoVM.
    // O Original.Invoke gerenciado apenas sincroniza este ponto de pausa.
    const auto result = universalHookMailbox().dispatch(
        slot, 0, [] { return true; }, wakeUniversalHookWorker,
        std::chrono::milliseconds(3), 0, false,
        universalNativeService.load(std::memory_order_acquire));
    if (result.accepted && !result.faulted) {
        stage.faults.store(0, std::memory_order_relaxed);
        return;
    }
    const int faults = stage.faults.fetch_add(1, std::memory_order_relaxed) + 1;
    const int64_t pauseMs = faults < 3 ? 24 : faults < 6 ? 250 : 1000;
    stage.retryAfterNs.store(now + pauseMs * 1000000, std::memory_order_relaxed);
}

extern "C" __attribute__((visibility("hidden"))) void blCSharpSystemStageDispatch(
    unsigned index, RegisterFrame*) {
    if (index < kCount) invokeStage(static_cast<int>(index));
}

void hkDoUpdateInWorld(void* self, const MethodInfo* info) {
    invokeStage(7);
    // Funciona mesmo com todos os callbacks C# desabilitados.
    // O método original roda EXATAMENTE uma vez no thread do jogo.
    originalWorldUpdate(self, info);
    invokeStage(8);
}
}

namespace {
Stage stages[kCount] = {
    {"PlayersBegin", 0x1b8, {0xd00100bd, 0xd001011a, 0xf00100f9, 0xd000ffdc}, csBridge0, &g_blCSharpSystemOriginal0},
    {"PlayersEnd", 0x57c, {0xd0010028, 0xf940b108, 0x2a1f03f5, 0xf9400108}, csBridge1, &g_blCSharpSystemOriginal1},
    {"NPCsBegin", 0x884, {0xaa1f03e0, 0x3900811f, 0x94063b0a, 0xaa1f03e0}, csBridge2, &g_blCSharpSystemOriginal2},
    {"NPCsEnd", 0xf40, {0xd0010028, 0xf940b108, 0x120002c9, 0xd00100f5}, csBridge3, &g_blCSharpSystemOriginal3},
    {"GoresEnd", 0x114c, {0x2a1f03f5, 0xf94003a0, 0xb940e008, 0x35000068}, csBridge4, &g_blCSharpSystemOriginal4},
    {"ItemsBegin", 0x1740, {0xd000feb7, 0xf000fed8, 0xd000ff99, 0x9001017c}, csBridge5, &g_blCSharpSystemOriginal5},
    {"ItemsEnd", 0x1a14, {0x9000ffc8, 0xf9437908, 0xb0010037, 0xd00100b6}, csBridge6, &g_blCSharpSystemOriginal6},
    {"EntitiesBegin", 0, {0, 0, 0, 0}, nullptr, nullptr},
    {"EntitiesEnd", 0, {0, 0, 0, 0}, nullptr, nullptr}
};

const MethodInfo* worldMethod() {
    Il2CppClass* main = il2cpp::findClassQuiet("Terraria", "Main");
    return main ? il2cpp::findMethodBySignature(
        main, il2cpp::parseSignature("void DoUpdateInWorld()")) : nullptr;
}
}
#endif

int installUniversalSystemStageOnGameThread(const std::string& name, int slot) {
#if !defined(__aarch64__)
    (void)name;
    (void)slot;
    return -2;
#else
    if (slot < 0 || slot >= 64) return -1;
    int index = -1;
    for (int i = 0; i < kCount; ++i)
        if (name == stages[i].name) { index = i; break; }
    if (index < 0) return -3;
    auto& stage = stages[index];
    if (stage.active.load(std::memory_order_acquire)) return -8;
    if (!rootInstalled.load(std::memory_order_acquire)) {
        const MethodInfo* method = worldMethod();
        if (!method || !il2cpp::methodPointer(method)) return -7;
        if (!hook::install(method, hkDoUpdateInWorld, &originalWorldUpdate)) return -9;
        rootInstalled.store(true, std::memory_order_release);
    }

    if (index < 7 && !*stage.original) {
        // Os quatro opcodes originais do Bunny validam exatamente a build
        // Terraria Android 301720. Falha segura para versões incompatíveis.
        const MethodInfo* method = worldMethod();
        const auto* code = method ?
            static_cast<const uint8_t*>(il2cpp::methodPointer(method)) : nullptr;
        if (!code || std::memcmp(code + stage.offset, stage.expected,
                                 sizeof(stage.expected)) != 0) return -6;
        if (!hook::install(const_cast<uint8_t*>(code + stage.offset),
                           reinterpret_cast<void*>(stage.bridge),
                           stage.original)) return -9;
    }
    stage.retryAfterNs.store(0, std::memory_order_relaxed);
    stage.faults.store(0, std::memory_order_relaxed);
    stage.slot.store(slot, std::memory_order_release);
    stage.active.store(true, std::memory_order_release);
    return 1000 + index;
#endif
}

void removeUniversalSystemStage(int handle) {
#if defined(__aarch64__)
    const int index = handle - 1000;
    if (index >= 0 && index < kCount) {
        stages[index].active.store(false, std::memory_order_release);
        stages[index].slot.store(-1, std::memory_order_release);
    }
#else
    (void)handle;
#endif
}

void disableAllUniversalSystemStages() {
#if defined(__aarch64__)
    for (auto& stage : stages) {
        stage.active.store(false, std::memory_order_release);
        stage.slot.store(-1, std::memory_order_release);
    }
#endif
}
}
