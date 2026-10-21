#include "experiments/csharp/UniversalSystemStages.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "experiments/csharp/UniversalHooks.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Api.h"
#include "il2cpp/Signature.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace bl::csharp {
#if defined(__aarch64__)
namespace {
using Clock = std::chrono::steady_clock;
constexpr int kCount = 12;
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
using UiUpdateFn = void (*)(void*, const MethodInfo*);
UiUpdateFn originalUiUpdate = nullptr;
FieldInfo* gameMenuField = nullptr;
std::atomic<bool> rootInstalled{false};
using TimeRateFn = void (*)(const MethodInfo*);
TimeRateFn originalTimeRate = nullptr;
struct NumericField { FieldInfo* field = nullptr; bool isDouble = false; };
NumericField dayRateField, tileRateField;
std::atomic<double> clockRate{1.0};
std::atomic<double> timeSnapshot{1.0}, tileSnapshot{1.0};
std::atomic<double> proposedTime{1.0}, proposedTiles{1.0}, proposedEvents{1.0};
std::atomic<int64_t> timeSession{0};
std::atomic<bool> timeLive{false}, timeModified{false};
double tileRemainder = 0, eventRemainder = 0;

bool validRate(double value) {
    return std::isfinite(value) && value >= 0 && value <= 2147483647.0;
}
bool numericField(Il2CppClass* parent, const char* name, NumericField& out) {
    auto& api = il2cpp::api();
    if (!api.field_get_type || !api.type_get_name || !api.il2cpp_free ||
        !api.field_static_get_value || !api.field_static_set_value) return false;
    FieldInfo* field = il2cpp::findField(parent, name);
    const Il2CppType* type = field ? api.field_get_type(field) : nullptr;
    char* text = type ? api.type_get_name(type) : nullptr;
    bool match = text && (!std::strcmp(text, "System.Double") ||
                          !std::strcmp(text, "System.Single"));
    const bool isDouble = text && !std::strcmp(text, "System.Double");
    if (text) api.il2cpp_free(text);
    if (!match) return false;
    out = {field, isDouble};
    return true;
}
double readRate(const NumericField& field) {
    auto& api = il2cpp::api();
    if (field.isDouble) {
        double rate = 0;
        api.field_static_get_value(field.field, &rate);
        return rate;
    }
    float rate = 0;
    api.field_static_get_value(field.field, &rate);
    return rate;
}
void writeRate(const NumericField& field, double rate) {
    auto& api = il2cpp::api();
    if (field.isDouble) {
        api.field_static_set_value(field.field, &rate);
    } else {
        float single = static_cast<float>(rate);
        api.field_static_set_value(field.field, &single);
    }
}

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
__attribute__((visibility("hidden"))) void* g_blCSharpSystemOriginal9 = nullptr;
}

void invokeStage(int index, uintptr_t context = 0) {
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
        slot, context, [] { return true; }, wakeUniversalHookWorker,
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
    unsigned index, RegisterFrame* frame) {
    if (index == 11) {
        if (!stages[11].active.load(std::memory_order_acquire) || !frame) return;
        const double rate = clockRate.load(std::memory_order_acquire);
        if (validRate(rate)) std::memcpy(frame->vectors[0], &rate, sizeof(rate));
        return;
    }
    if (index < kCount) invokeStage(static_cast<int>(index));
}

void hkUpdateTimeRate(const MethodInfo* info) {
    originalTimeRate(info);
    if (!dayRateField.field || !tileRateField.field) return;
    const double time = readRate(dayRateField);
    const double tiles = readRate(tileRateField);
    if (!validRate(time) || !validRate(tiles)) return;

    timeSnapshot.store(time, std::memory_order_relaxed);
    tileSnapshot.store(tiles, std::memory_order_relaxed);
    proposedTime.store(time, std::memory_order_relaxed);
    proposedTiles.store(tiles, std::memory_order_relaxed);
    proposedEvents.store(time, std::memory_order_relaxed);
    timeModified.store(false, std::memory_order_release);
    timeSession.fetch_add(1, std::memory_order_acq_rel);
    timeLive.store(true, std::memory_order_release);
    invokeStage(10);
    timeLive.store(false, std::memory_order_release);

    double requestedTime = time;
    double requestedTiles = tiles;
    double requestedEvents = time;
    if (timeModified.exchange(false, std::memory_order_acq_rel)) {
        requestedTime = proposedTime.load(std::memory_order_acquire);
        requestedTiles = proposedTiles.load(std::memory_order_acquire);
        requestedEvents = proposedEvents.load(std::memory_order_acquire);
    }
    if (!validRate(requestedTime)) requestedTime = time;
    if (!validRate(requestedTiles)) requestedTiles = tiles;
    if (!validRate(requestedEvents)) requestedEvents = time;
    clockRate.store(requestedTime, std::memory_order_release);
    double tilesWithFraction = requestedTiles + tileRemainder;
    double eventsWithFraction = requestedEvents + eventRemainder;
    tilesWithFraction = std::min(2147483647.0, tilesWithFraction);
    eventsWithFraction = std::min(2147483647.0, eventsWithFraction);
    writeRate(tileRateField, std::floor(tilesWithFraction));
    writeRate(dayRateField, std::floor(eventsWithFraction));
    tileRemainder = tilesWithFraction - std::floor(tilesWithFraction);
    eventRemainder = eventsWithFraction - std::floor(eventsWithFraction);
}

void hkUpdateUIStates(void* gameTime, const MethodInfo* info) {
    originalUiUpdate(gameTime, info);
    bool gameMenu = true;
    if (!gameMenuField || !il2cpp::api().field_static_get_value) return;
    il2cpp::api().field_static_get_value(gameMenuField, &gameMenu);
    if (!gameMenu) invokeStage(9, reinterpret_cast<uintptr_t>(gameTime));
}

void hkDoUpdateInWorld(void* self, const MethodInfo* info) {
    invokeStage(7);
    // Funciona mesmo com todos os callbacks C# desabilitados.
    // O método original roda EXATAMENTE uma vez no thread do jogo.
    originalWorldUpdate(self, info);
    invokeStage(8);
}
}

#define BL_CSHARP_SYSTEM_SAVE_FRAME \
"sub sp, sp, #768\n" \
"stp x0, x1, [sp, #0]\n" \
"stp x2, x3, [sp, #16]\n" \
"stp x4, x5, [sp, #32]\n" \
"stp x6, x7, [sp, #48]\n" \
"stp x8, x9, [sp, #64]\n" \
"stp x10, x11, [sp, #80]\n" \
"stp x12, x13, [sp, #96]\n" \
"stp x14, x15, [sp, #112]\n" \
"stp x16, x17, [sp, #128]\n" \
"stp x18, x19, [sp, #144]\n" \
"stp x20, x21, [sp, #160]\n" \
"stp x22, x23, [sp, #176]\n" \
"stp x24, x25, [sp, #192]\n" \
"stp x26, x27, [sp, #208]\n" \
"stp x28, x29, [sp, #224]\n" \
"str x30, [sp, #240]\n" \
"mrs x9, nzcv\n" \
"str x9, [sp, #248]\n" \
"stp q0, q1, [sp, #256]\n" \
"stp q2, q3, [sp, #288]\n" \
"stp q4, q5, [sp, #320]\n" \
"stp q6, q7, [sp, #352]\n" \
"stp q8, q9, [sp, #384]\n" \
"stp q10, q11, [sp, #416]\n" \
"stp q12, q13, [sp, #448]\n" \
"stp q14, q15, [sp, #480]\n" \
"stp q16, q17, [sp, #512]\n" \
"stp q18, q19, [sp, #544]\n" \
"stp q20, q21, [sp, #576]\n" \
"stp q22, q23, [sp, #608]\n" \
"stp q24, q25, [sp, #640]\n" \
"stp q26, q27, [sp, #672]\n" \
"stp q28, q29, [sp, #704]\n" \
"stp q30, q31, [sp, #736]\n"

#define BL_CSHARP_SYSTEM_RESTORE_FRAME \
"ldp q0, q1, [sp, #256]\n" \
"ldp q2, q3, [sp, #288]\n" \
"ldp q4, q5, [sp, #320]\n" \
"ldp q6, q7, [sp, #352]\n" \
"ldp q8, q9, [sp, #384]\n" \
"ldp q10, q11, [sp, #416]\n" \
"ldp q12, q13, [sp, #448]\n" \
"ldp q14, q15, [sp, #480]\n" \
"ldp q16, q17, [sp, #512]\n" \
"ldp q18, q19, [sp, #544]\n" \
"ldp q20, q21, [sp, #576]\n" \
"ldp q22, q23, [sp, #608]\n" \
"ldp q24, q25, [sp, #640]\n" \
"ldp q26, q27, [sp, #672]\n" \
"ldp q28, q29, [sp, #704]\n" \
"ldp q30, q31, [sp, #736]\n" \
"ldr x9, [sp, #248]\n" \
"msr nzcv, x9\n" \
"ldp x0, x1, [sp, #0]\n" \
"ldp x2, x3, [sp, #16]\n" \
"ldp x4, x5, [sp, #32]\n" \
"ldp x6, x7, [sp, #48]\n" \
"ldp x8, x9, [sp, #64]\n" \
"ldp x10, x11, [sp, #80]\n" \
"ldp x12, x13, [sp, #96]\n" \
"ldp x14, x15, [sp, #112]\n" \
"ldp x16, x17, [sp, #128]\n" \
"ldp x18, x19, [sp, #144]\n" \
"ldp x20, x21, [sp, #160]\n" \
"ldp x22, x23, [sp, #176]\n" \
"ldp x24, x25, [sp, #192]\n" \
"ldp x26, x27, [sp, #208]\n" \
"ldp x28, x29, [sp, #224]\n" \
"ldr x30, [sp, #240]\n" \
"add sp, sp, #768\n"

#define BL_CSHARP_SYSTEM_BRIDGE(name, stage, target) \
__attribute__((naked)) void name() { \
    __asm__ volatile(BL_CSHARP_SYSTEM_SAVE_FRAME \
        "mov x1, sp\n" "mov w0, #" #stage "\n" "bl blCSharpSystemStageDispatch\n" \
        BL_CSHARP_SYSTEM_RESTORE_FRAME \
        "adrp x16, " #target "\n" "ldr x16, [x16, :lo12:" #target "]\n" "br x16\n"); \
}

BL_CSHARP_SYSTEM_BRIDGE(csBridge0, 0, g_blCSharpSystemOriginal0)
BL_CSHARP_SYSTEM_BRIDGE(csBridge1, 1, g_blCSharpSystemOriginal1)
BL_CSHARP_SYSTEM_BRIDGE(csBridge2, 2, g_blCSharpSystemOriginal2)
BL_CSHARP_SYSTEM_BRIDGE(csBridge3, 3, g_blCSharpSystemOriginal3)
BL_CSHARP_SYSTEM_BRIDGE(csBridge4, 4, g_blCSharpSystemOriginal4)
BL_CSHARP_SYSTEM_BRIDGE(csBridge5, 5, g_blCSharpSystemOriginal5)
BL_CSHARP_SYSTEM_BRIDGE(csBridge6, 6, g_blCSharpSystemOriginal6)
BL_CSHARP_SYSTEM_BRIDGE(csBridge7, 7, g_blCSharpSystemOriginal7)
BL_CSHARP_SYSTEM_BRIDGE(csBridge8, 8, g_blCSharpSystemOriginal8)
BL_CSHARP_SYSTEM_BRIDGE(csBridge9, 11, g_blCSharpSystemOriginal9)

#undef BL_CSHARP_SYSTEM_BRIDGE
#undef BL_CSHARP_SYSTEM_RESTORE_FRAME
#undef BL_CSHARP_SYSTEM_SAVE_FRAME

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
    {"EntitiesEnd", 0, {0, 0, 0, 0}, nullptr, nullptr},
    {"UIEnd", 0, {0, 0, 0, 0}, nullptr, nullptr},
    {"TimeRate", 0, {0, 0, 0, 0}, nullptr, nullptr},
    {"ClockAdvance", 0xb8c, {0x1e602960, 0xfd024500, 0x97efb85d, 0xf94002e0}, csBridge9, &g_blCSharpSystemOriginal9}
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
    if (index < 9 && !rootInstalled.load(std::memory_order_acquire)) {
        const MethodInfo* method = worldMethod();
        if (!method || !il2cpp::methodPointer(method)) return -7;
        if (!hook::install(method, hkDoUpdateInWorld, &originalWorldUpdate)) return -9;
        rootInstalled.store(true, std::memory_order_release);
    }

    if (index == 9 && !originalUiUpdate) {
        auto* main = il2cpp::findClassQuiet("Terraria", "Main");
        const MethodInfo* method = main ? il2cpp::findMethodBySignature(
            main, il2cpp::parseSignature("void UpdateUIStates(GameTime gameTime)")) : nullptr;
        FieldInfo* menu = main ? il2cpp::findField(main, "gameMenu") : nullptr;
        if (!method || !il2cpp::methodPointer(method) || !menu ||
            !il2cpp::api().field_static_get_value) return -7;
        if (!hook::install(method, hkUpdateUIStates, &originalUiUpdate)) return -9;
        gameMenuField = menu;
    }
    if (index == 10 && !originalTimeRate) {
        auto* main = il2cpp::findClassQuiet("Terraria", "Main");
        const MethodInfo* method = main ? il2cpp::findMethodBySignature(
            main, il2cpp::parseSignature("void UpdateTimeRate()")) : nullptr;
        NumericField day, tiles;
        if (!method || !il2cpp::methodPointer(method) || !main ||
            !numericField(main, "dayRate", day) ||
            !numericField(main, "desiredWorldTilesUpdateRate", tiles)) return -7;

        auto* update = il2cpp::findMethodBySignature(main,
            il2cpp::parseSignature("void UpdateTime()"));
        const auto* code = update ?
            static_cast<const uint8_t*>(il2cpp::methodPointer(update)) : nullptr;
        auto& clock = stages[11];
        if (!code) return -7;
        if (!*clock.original) {
            if (std::memcmp(code + clock.offset, clock.expected,
                            sizeof(clock.expected)) != 0) return -6;
            if (!hook::install(const_cast<uint8_t*>(code + clock.offset),
                               reinterpret_cast<void*>(clock.bridge),
                               clock.original)) return -9;
        }
        dayRateField = day;
        tileRateField = tiles;
        if (!hook::install(method, hkUpdateTimeRate, &originalTimeRate)) return -9;
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
    if (index == 10) stages[11].active.store(true, std::memory_order_release);
    return 1000 + index;
#endif
}

void removeUniversalSystemStage(int handle) {
#if defined(__aarch64__)
    const int index = handle - 1000;
    if (index >= 0 && index < kCount) {
        stages[index].active.store(false, std::memory_order_release);
        stages[index].slot.store(-1, std::memory_order_release);
        if (index == 10) { stages[11].active.store(false, std::memory_order_release); timeLive.store(false, std::memory_order_release); }
    }
#else
    (void)handle;
#endif
}

extern "C" __attribute__((visibility("default"))) int bunny_system_time_rate_read(
    int64_t* session, double* time, double* tiles, double* events) {
#if defined(__aarch64__)
    if (!session || !time || !tiles || !events ||
        !timeLive.load(std::memory_order_acquire)) return 0;
    *session = timeSession.load(std::memory_order_acquire);
    *time = timeSnapshot.load(std::memory_order_acquire);
    *tiles = tileSnapshot.load(std::memory_order_acquire);
    *events = *time;
    return 1;
#else
    (void)session; (void)time; (void)tiles; (void)events;
    return 0;
#endif
}

extern "C" __attribute__((visibility("default"))) int bunny_system_time_rate_write(
    int64_t session, double time, double tiles, double events) {
#if defined(__aarch64__)
    if (!validRate(time) || !validRate(tiles) || !validRate(events) ||
        !timeLive.load(std::memory_order_acquire) ||
        session != timeSession.load(std::memory_order_acquire)) return 0;
    proposedTime.store(time, std::memory_order_relaxed);
    proposedTiles.store(tiles, std::memory_order_relaxed);
    proposedEvents.store(events, std::memory_order_relaxed);
    timeModified.store(true, std::memory_order_release);
    return 1;
#else
    (void)session; (void)time; (void)tiles; (void)events;
    return 0;
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
