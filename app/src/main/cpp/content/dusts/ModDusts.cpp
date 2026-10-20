#include "content/dusts/ModDusts.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ManagedDustLifecycle.h"
#include "experiments/csharp/ManagedDustDecision.h"
#include "experiments/csharp/ManagedDustFrameBudget.h"
#include "experiments/csharp/ManagedDustSlotLookup.h"
#include "content/common/ContentAssets.h"
#include "content/common/GameRefs.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>

namespace bl::runtime {

namespace {

// Main.dust tem 6001 posicoes; a 6000 e a "nenhuma" que o NewDust devolve
// quando nao ha vaga, e nunca entra na pilha (SetNotActive a ignora).
constexpr int kDustSlots = 6000;
// Sem poeira de mod, a pilha e conferida por inteiro a cada tantos quadros.
constexpr int kIdleScanEvery = 30;

struct Vec2 { float x, y; };
struct Rect { int32_t x, y, w, h; };
struct Color32 { uint32_t packed; };

struct Entry {
    ModDustDef def;
    uint32_t asset = 0;   // gchandle do Asset<Texture2D> (0 = a folha do jogo)
    int updateType = -1;
};

std::mutex g_mx;
std::vector<Entry> g_regs;   // indice = type - kVanillaDustCount
std::atomic<int> g_total{0};
std::atomic<int> g_installed{0};
bool g_failed = false;
DustCallbacks g_cb;

// Por tipo de mod, copiados na instalacao (thread do jogo; leitura nos hooks).
std::vector<unsigned> g_hooks;
std::vector<int> g_updateType;
std::vector<int> g_group;           // 0 = folha do jogo; k = g_groupAssets[k - 1]
std::vector<uint32_t> g_groupAssets;
unsigned g_anyHooks = 0;
std::atomic<bool> g_maskDirty{false};
bool g_alphaRefHooked = false;
bool g_alphaValueHooked = false;

TypeTables g_tables("poeiras de mod", kVanillaDustCount, {
    {"Terraria", "Dust", ""},
    {"Terraria.GameContent", "ChildSafety", ""},
}, TypeTables::Fill::Mode);

struct Refs {
    bool tried = false, ok = false;
    FieldInfo* mainDust = nullptr;        // Main.dust
    FieldInfo* maxDustToDraw = nullptr;   // Main.maxDustToDraw
    FieldInfo* freeDust = nullptr;        // Dust.freeDust
    FieldInfo* freeDustCount = nullptr;   // Dust.freeDustCount
    FieldInfo* activeDustCount = nullptr; // Dust.activeDustCount
    FieldInfo* staticAlpha = nullptr;     // Dust.StaticAlpha
    FieldInfo* dustCount = nullptr;       // DustID.Count
    FieldInfo* texture = nullptr;         // TextureAssets.Dust
    int32_t active = -1, type = -1, frame = -1, velocity = -1, fullBright = -1;
    int32_t dustIndex = -1, position = -1, scale = -1, rotation = -1, alpha = -1, color = -1;
    const MethodInfo* newDust = nullptr;
    const MethodInfo* updateDust = nullptr;
    const MethodInfo* drawDust = nullptr;
    const MethodInfo* getAlphaRef = nullptr;
    const MethodInfo* getAlphaValue = nullptr;
    const MethodInfo* setNotActive = nullptr;
};

Refs& refs() {
    static Refs r;
    if (r.tried) return r;
    r.tried = true;
    using namespace il2cpp;
    auto& a = api();
    Il2CppClass* dust = findClass({"Terraria", "Dust", {}});
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* ids = findClass({"Terraria.ID", "DustID", {}});
    Il2CppClass* textures = findClass({"Terraria.GameContent", "TextureAssets", {}});
    if (!dust || !main || !ids || !textures) {
        BL_ERROR("poeiras de mod: classes faltando (Dust=%p Main=%p DustID=%p TextureAssets=%p)",
                 (void*)dust, (void*)main, (void*)ids, (void*)textures);
        return r;
    }
    r.mainDust = findField(main, "dust");
    r.maxDustToDraw = findField(main, "maxDustToDraw");
    r.freeDust = findField(dust, "freeDust");
    r.freeDustCount = findField(dust, "freeDustCount");
    r.activeDustCount = findField(dust, "activeDustCount");
    r.staticAlpha = findField(dust, "StaticAlpha");
    r.dustCount = findField(ids, "Count");
    r.texture = findField(textures, "Dust");
    r.active = fieldOffset(dust, "active");
    r.type = fieldOffset(dust, "type");
    r.frame = fieldOffset(dust, "frame");
    r.velocity = fieldOffset(dust, "velocity");
    r.fullBright = fieldOffset(dust, "fullBright");
    r.dustIndex = fieldOffset(dust, "dustIndex");
    r.position = fieldOffset(dust, "position");
    r.scale = fieldOffset(dust, "scale");
    r.rotation = fieldOffset(dust, "rotation");
    r.alpha = fieldOffset(dust, "alpha");
    r.color = fieldOffset(dust, "color");

    void* it = nullptr;
    while (const MethodInfo* m = a.class_get_methods(dust, &it)) {
        const char* n = a.method_get_name(m);
        const uint32_t params = a.method_get_param_count(m);
        if (std::strcmp(n, "NewDust") == 0 && params == 9) r.newDust = m;
        else if (std::strcmp(n, "UpdateDust") == 0 && params == 0) r.updateDust = m;
        else if (std::strcmp(n, "SetNotActive") == 0 && params == 1) r.setNotActive = m;
        else if (std::strcmp(n, "GetAlpha") == 0 && params == 1) {
            // GetAlpha(ref Color) e void; GetAlpha(Color) devolve a cor.
            const Il2CppType* ret = a.method_get_return_type(m);
            char* name = ret ? a.type_get_name(ret) : nullptr;
            const bool isVoid = name && std::strcmp(name, "System.Void") == 0;
            if (name) a.il2cpp_free(name);
            (isVoid ? r.getAlphaRef : r.getAlphaValue) = m;
        }
    }
    r.drawDust = a.class_get_method_from_name(main, "DrawDust", 0);

    r.ok = r.mainDust && r.maxDustToDraw && r.freeDust && r.freeDustCount && r.activeDustCount &&
           r.staticAlpha && r.dustCount && r.texture && r.active >= 0 && r.type >= 0 && r.frame >= 0 &&
           r.velocity >= 0 && r.fullBright >= 0 && r.newDust && r.updateDust && r.drawDust &&
           r.getAlphaRef && r.getAlphaValue && r.setNotActive;
    if (!r.ok) {
        BL_ERROR("poeiras de mod: refs faltando (Main.dust=%p freeDust=%p NewDust=%p UpdateDust=%p "
                 "DrawDust=%p GetAlpha=%p/%p SetNotActive=%p)",
                 (void*)r.mainDust, (void*)r.freeDust, (void*)r.newDust, (void*)r.updateDust,
                 (void*)r.drawDust, (void*)r.getAlphaRef, (void*)r.getAlphaValue, (void*)r.setNotActive);
    }
    return r;
}

template <typename T>
T readStatic(FieldInfo* f) {
    T v{};
    il2cpp::api().field_static_get_value(f, &v);
    return v;
}

void writeStaticInt(FieldInfo* f, int32_t v) {
    il2cpp::api().field_static_set_value(f, &v);
}

Il2CppObject** dustSlots(Il2CppArray* arr) {
    return static_cast<Il2CppObject**>(arrayData(arr));
}

bool& activeOf(Il2CppObject* d) { return field<bool>(d, refs().active); }
int32_t& typeOf(Il2CppObject* d) { return field<int32_t>(d, refs().type); }

int modIndex(int type) {
    const int i = type - kVanillaDustCount;
    return i >= 0 && i < g_installed.load(std::memory_order_relaxed) ? i : -1;
}

using SetNotActiveFn = void (*)(Il2CppObject*, const MethodInfo*);

void setNotActive(Il2CppObject* d) {
    const Refs& r = refs();
    reinterpret_cast<SetNotActiveFn>(il2cpp::methodPointer(r.setNotActive))(d, r.setNotActive);
}

// ---- a pilha de poeiras livres ----

std::atomic<int> g_repairs{0};
DustPoolStats g_lastStats;

/**
 * Refaz Dust.freeDust com toda poeira inativa de Main.dust[0..6000), na ordem
 * do Dust.Reset (o topo e a de menor indice), e acerta os dois contadores.
 */
void rebuildPool(Il2CppArray* dusts, int active) {
    const Refs& r = refs();
    Il2CppArray* free = readStatic<Il2CppArray*>(r.freeDust);
    if (!free || free->length < static_cast<uintptr_t>(kDustSlots + 1)) return;
    Il2CppObject** slots = dustSlots(dusts);
    auto** stack = static_cast<Il2CppObject**>(arrayData(free));
    auto& a = il2cpp::api();
    int n = 0;
    for (int i = kDustSlots - 1; i >= 0; --i) {
        Il2CppObject* d = slots[i];
        if (!d || activeOf(d)) continue;
        ++n;
        if (a.gc_wbarrier_set_field) a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(free),
                                                             reinterpret_cast<void**>(&stack[n]), d);
        else stack[n] = d;
    }
    writeStaticInt(r.freeDustCount, n);
    writeStaticInt(r.activeDustCount, active);
    const int repairs = g_repairs.fetch_add(1) + 1;
    if (repairs <= 5 || repairs % 100 == 0) {
        BL_WARN("poeiras: pilha de poeiras livres refeita (%d livres, %d ativas; vez %d). "
                "Algum mod escreveu dust.active direto em vez de chamar Dust.SetNotActive", n, active, repairs);
    }
}

/** A poeira do topo da pilha (a proxima que o NewDust pega) tem de estar livre. */
bool poolTopBroken() {
    const Refs& r = refs();
    const int count = readStatic<int32_t>(r.freeDustCount);
    if (count < 0 || count > kDustSlots) return true;
    if (count == 0) return false;
    Il2CppArray* free = readStatic<Il2CppArray*>(r.freeDust);
    if (!free || static_cast<uintptr_t>(count) >= free->length) return true;
    Il2CppObject* top = static_cast<Il2CppObject**>(arrayData(free))[count];
    return !top || activeOf(top);
}

/** Toda poeira ativa e, das de mod abaixo de `limit`, as listas. */
struct Scan {
    int active = 0;
    std::vector<Il2CppObject*> all;    // ativas abaixo de `limit`
    std::vector<Il2CppObject*> mods;   // ativas de mod abaixo de `limit`
};

void scanDusts(Il2CppArray* dusts, int limit, Scan& out, bool collect) {
    out.active = 0;
    out.all.clear();
    out.mods.clear();
    Il2CppObject** slots = dustSlots(dusts);
    for (int i = 0; i < kDustSlots; ++i) {
        Il2CppObject* d = slots[i];
        if (!d || !activeOf(d)) continue;
        ++out.active;
        if (!collect || i >= limit) continue;
        out.all.push_back(d);
        if (modIndex(typeOf(d)) >= 0) out.mods.push_back(d);
    }
}

void checkPool(Il2CppArray* dusts, int active) {
    const int counted = readStatic<int32_t>(refs().activeDustCount);
    if (counted != active || poolTopBroken()) rebuildPool(dusts, active);
}

// Todas as vagas, e nao so as abaixo do Main.maxDustToDraw: o jogo muda esse
// limite sozinho (com o desempenho), e uma poeira de mod numa vaga alta que o
// jogo ainda atualiza e desenha ficava sem o Update e com a textura errada.
int scanLimit() {
    return kDustSlots;
}

// ---- hooks ----

using NewDustFn = int32_t (*)(Vec2, int32_t, int32_t, int32_t, float, float, int32_t, Color32, float,
                              const MethodInfo*);
using VoidStaticFn = void (*)(const MethodInfo*);
using VoidInstanceFn = void (*)(Il2CppObject*, const MethodInfo*);
using GetAlphaRefFn = void (*)(Il2CppObject*, Color32*, const MethodInfo*);
using GetAlphaValueFn = Color32 (*)(Il2CppObject*, Color32, const MethodInfo*);

NewDustFn g_origNewDust = nullptr;
VoidStaticFn g_origUpdateDust = nullptr;
VoidInstanceFn g_origDrawDust = nullptr;
GetAlphaRefFn g_origGetAlphaRef = nullptr;
GetAlphaValueFn g_origGetAlphaValue = nullptr;

int g_frame = 0;
std::array<int, kDustSlots> g_dustGenerations{};
csharp::ManagedDustLifecycle g_dustLifecycle;
csharp::ManagedDustFrameBudget g_dustFrameBudget;
int64_t g_dustSyncAttempts = 0, g_dustSyncReady = 0, g_dustSyncMissed = 0;
int64_t g_dustSyncWaitNs = 0;
csharp::ManagedDustSlotLookup<Il2CppObject, kDustSlots> g_dustSlotLookup;
Scan g_scan;
std::vector<Il2CppObject*> g_list;
std::vector<bool> g_flags;
std::vector<Il2CppObject*> g_hidden;
struct Swapped { Il2CppObject* dust; int realType, updateType; };
std::vector<Swapped> g_swapped;

// Enquanto o JS roda: o menor freeDustCount visto (o NewDust pega do topo).
// As vagas de [menor + 1 .. agora] sao as que voltaram a pilha nesse tempo.
bool g_tracking = false;
int g_minFree = 0;

int64_t g_updateNs = 0, g_drawNs = 0, g_jsNs = 0, g_updates = 0, g_draws = 0, g_groupPasses = 0, g_groupDusts = 0;

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/** Soma o tempo do bloco em `total` ao sair. */
struct Timed {
    int64_t& total;
    int64_t start = nowNs();
    explicit Timed(int64_t& t) : total(t) {}
    ~Timed() { total += nowNs() - start; }
};

void beginKillTracking() {
    g_jsNs -= nowNs();
    g_minFree = readStatic<int32_t>(refs().freeDustCount);
    g_tracking = true;
}

/**
 * O padrao do tModLoader e `dust.active = false` dentro do Update: no celular
 * isso deixa a vaga fora da pilha. A poeira de `list` que ficou inativa sem
 * voltar a pilha volta pelo Dust.SetNotActive, como o jogo faria.
 */
void endKillTracking(const std::vector<Il2CppObject*>& list) {
    g_tracking = false;
    g_jsNs += nowNs();
    const Refs& r = refs();
    const int now = readStatic<int32_t>(r.freeDustCount);
    Il2CppArray* free = readStatic<Il2CppArray*>(r.freeDust);
    if (!free) return;
    auto** stack = static_cast<Il2CppObject**>(arrayData(free));
    const int lo = g_minFree < 0 ? 0 : g_minFree;
    const int hi = now < static_cast<int>(free->length) ? now : static_cast<int>(free->length) - 1;
    for (Il2CppObject* d : list) {
        if (activeOf(d)) continue;
        bool returned = false;
        for (int k = lo + 1; k <= hi && !returned; ++k) returned = stack[k] == d;
        if (returned) continue;
        activeOf(d) = true;
        setNotActive(d);
    }
}

// Como o DustLoader.SetupDust: a moldura no topo da textura propria (o jogo
// a calculou pela posicao do tipo na folha) e o OnSpawn.
bool managedFieldsReady() {
    const Refs& r = refs();
    return r.position > 0 && r.scale > 0 &&
           r.rotation > 0 && r.alpha > 0 && r.color > 0;
}

int managedIndex(Il2CppObject* dust, int knownSlot = -1) {
    if (!dust) return -1;
    if (!managedFieldsReady()) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            BL_ERROR("[CSharp][DustLoader] Stage=ResolveFields Result=Rejected Reason=MissingNativeDustOffsets");
        }
        return -1;
    }
    Il2CppArray* array = readStatic<Il2CppArray*>(refs().mainDust);
    if (!array || array->length < static_cast<uintptr_t>(kDustSlots))
        return -1;
    const int offset = refs().dustIndex;
    int hinted = knownSlot;
    if (hinted < 0 && offset > 0)
        hinted = field<int32_t>(dust, offset);
    const int index = g_dustSlotLookup.resolve(
        dustSlots(array), static_cast<size_t>(array->length), dust, hinted);
    if (index >= 0 && hinted != index) {
        static bool reportedFallback = false;
        if (!reportedFallback) {
            reportedFallback = true;
            BL_WARN("[CSharp][DustLoader] Stage=ResolveFields Result=FallbackToMainDustSlots");
        }
    }
    return index;
}

csharp::ManagedDustState captureManagedDust(Il2CppObject* dust, int slot, int generation) {
    const Refs& r = refs();
    const Vec2& position = field<Vec2>(dust, r.position);
    const Vec2& velocity = field<Vec2>(dust, r.velocity);
    const Rect& rect = field<Rect>(dust, r.frame);
    csharp::ManagedDustState state{};
    state.slot = slot;
    state.type = typeOf(dust);
    state.generation = generation;
    state.active = activeOf(dust) ? 1 : 0;
    state.x = position.x;
    state.y = position.y;
    state.velocityX = velocity.x;
    state.velocityY = velocity.y;
    state.scale = field<float>(dust, r.scale);
    state.rotation = field<float>(dust, r.rotation);
    state.alpha = field<int32_t>(dust, r.alpha);
    state.frameX = rect.x;
    state.frameY = rect.y;
    state.frameWidth = rect.w;
    state.frameHeight = rect.h;
    state.color = field<Color32>(dust, r.color).packed;
    return state;
}

void applyManagedDust(Il2CppObject* dust, const csharp::ManagedDustState& before,
                      const csharp::ManagedDustState& state, bool updateColor = true) {
    if (!activeOf(dust)) return;
    const int slot = managedIndex(dust, before.slot);
    if (slot < 0 || slot != before.slot) return;
    const auto live = captureManagedDust(dust, slot, state.generation);
    csharp::ManagedDustState merged{};
    if (!csharp::mergeManagedDustDelta(live, before, state, &merged, updateColor))
        return;
    if (merged.active == 0) {
        setNotActive(dust);
        return;
    }
    const Refs& r = refs();
    Vec2& pos = field<Vec2>(dust, r.position);
    Vec2& velocity = field<Vec2>(dust, r.velocity);
    pos = {merged.x, merged.y};
    velocity = {merged.velocityX, merged.velocityY};
    field<float>(dust, r.scale) = merged.scale;
    field<float>(dust, r.rotation) = merged.rotation;
    field<int32_t>(dust, r.alpha) = merged.alpha;
    Rect& rect = field<Rect>(dust, r.frame);
    rect = {merged.frameX, merged.frameY, merged.frameWidth, merged.frameHeight};
    if (updateColor) field<Color32>(dust, r.color).packed = merged.color;
}

struct ManagedDustCached {
    int generation = 0;
    int type = -1;
    std::uint64_t frame = 0;
    std::uint32_t flags = 0;
    std::uint32_t color = 0;
    std::uint32_t requestColor = 0;
    bool ready = false;
};
std::array<ManagedDustCached, kDustSlots * csharp::ManagedDustMailbox::kPhases> g_dustCached{};

void sweepManagedDustLifecycle(Il2CppArray* dusts) {
    if (!dusts || dusts->length < static_cast<uintptr_t>(kDustSlots)) return;
    auto** all = dustSlots(dusts);
    g_dustLifecycle.sweep([&](int slot, int type, int generation) {
        Il2CppObject* dust = all[slot];
        return dust && activeOf(dust) && typeOf(dust) == type &&
               g_dustGenerations[static_cast<size_t>(slot)] == generation;
    });
    g_dustLifecycle.flush(24, [&](const csharp::ManagedDustState& state) {
        if (!csharp::hasManagedDustType(state.type)) return true;
        return csharp::queueManagedDustCallback(
            state.type, 5, state, static_cast<std::uint64_t>(g_frame));
    });
    static std::uint64_t reportedDrops = 0;
    const auto dropped = g_dustLifecycle.droppedCount();
    if (dropped < reportedDrops) reportedDrops = 0;
    if (dropped > reportedDrops && (reportedDrops == 0 || dropped - reportedDrops >= 64)) {
        reportedDrops = dropped;
        BL_ERROR("[CSharp][DustLoader] Stage=ReleaseQueue Dropped=%llu Reason=QueueCapacityExceeded",
                 static_cast<unsigned long long>(dropped));
    }
}

bool managedDustPhase(Il2CppObject* dust, int phase, std::uint32_t* flags,
                      std::uint32_t* argColor = nullptr) {
    if (!dust || !activeOf(dust) || phase < 0 ||
        phase >= static_cast<int>(csharp::ManagedDustMailbox::kPhases) ||
        !csharp::hasManagedDustType(typeOf(dust))) return false;
    const int slot = managedIndex(dust);
    if (slot < 0) return false;
    int& generation = g_dustGenerations[static_cast<size_t>(slot)];
    if (generation <= 0) generation = 1;
    auto& cache = g_dustCached[static_cast<size_t>(slot) *
        csharp::ManagedDustMailbox::kPhases + static_cast<size_t>(phase)];
    if (cache.generation != generation || cache.type != typeOf(dust))
        cache = {};
    auto applyResponse = [&](const csharp::ManagedDustMailbox::Response& response) {
        applyManagedDust(dust, response.before, response.state, phase != 3);
        cache.generation = generation;
        cache.type = response.state.type;
        cache.frame = response.frame;
        cache.flags = response.flags;
        cache.color = response.state.color;
        cache.requestColor = response.before.color;
        cache.ready = true;
    };
    csharp::ManagedDustMailbox::Response response{};
    if (csharp::receiveManagedDustCallback(typeOf(dust), phase, slot, generation,
                                            static_cast<uint64_t>(g_frame), &response))
        applyResponse(response);
    if (phase == 0 && cache.ready &&
        cache.generation == generation && cache.type == typeOf(dust)) {
        if (flags) *flags = cache.flags;
        return true;
    }
    const auto validDecision = [&]() {
        return cache.ready &&
            cache.generation == generation && cache.type == typeOf(dust) &&
            csharp::managedDustDecisionCurrent(
                phase, static_cast<std::uint64_t>(g_frame), cache.frame,
                cache.requestColor, argColor ? *argColor : 0u);
    };
    bool valid = validDecision();
    if (activeOf(dust)) {
        auto state = captureManagedDust(dust, slot, generation);
        g_dustLifecycle.observe(state);
        if (!valid) {
            if (argColor) state.color = *argColor;
            const bool requiresSameFrame = phase >= 0 && phase <= 4;
            std::chrono::steady_clock::time_point deadline{};
            bool attempted = false;
            if (requiresSameFrame && csharp::managedDustWorkerReady(state.type) &&
                g_dustFrameBudget.deadline(static_cast<std::uint64_t>(g_frame), &deadline)) {
                attempted = true;
                csharp::ManagedDustMailbox::Response immediate{};
                const auto started = std::chrono::steady_clock::now();
                const bool ready = csharp::invokeManagedDustSameFrame(
                    state.type, phase, state, static_cast<std::uint64_t>(g_frame),
                    deadline, &immediate);
                g_dustFrameBudget.finish();
                ++g_dustSyncAttempts;
                g_dustSyncWaitNs +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started).count();
                if (ready) {
                    ++g_dustSyncReady;
                    applyResponse(immediate);
                    valid = validDecision();
                } else {
                    ++g_dustSyncMissed;
                }
            }
            if (!attempted)
                csharp::queueManagedDustCallback(state.type, phase, state,
                                                  static_cast<std::uint64_t>(g_frame));
        }
    }
    if (valid) {
        if (flags) *flags = cache.flags;
        if (argColor && (cache.flags & 8u) != 0u) *argColor = cache.color;
    }
    return valid;
}

int32_t hkNewDust(Vec2 pos, int32_t w, int32_t h, int32_t type, float sx, float sy, int32_t alpha,
                  Color32 color, float scale, const MethodInfo* m) {
    const int32_t index = g_origNewDust(pos, w, h, type, sx, sy, alpha, color, scale, m);
    if (g_tracking) {
        const int free = readStatic<int32_t>(refs().freeDustCount);
        if (free < g_minFree) g_minFree = free;
    }
    if (type < kVanillaDustCount || index < 0 || index >= kDustSlots) return index;
    g_dustLifecycle.retire(index);
    int& instance = g_dustGenerations[static_cast<size_t>(index)];
    if (instance <= 0 || instance == INT32_MAX) instance = 1;
    else ++instance;
    for (size_t stage = 0; stage < csharp::ManagedDustMailbox::kPhases; ++stage)
        g_dustCached[static_cast<size_t>(index) *
                     csharp::ManagedDustMailbox::kPhases + stage] = {};
    Il2CppArray* dusts = readStatic<Il2CppArray*>(refs().mainDust);
    if (!dusts) return index;
    Il2CppObject* d = dustSlots(dusts)[index];
    if (d && csharp::hasManagedDustType(typeOf(d)))
        g_dustSlotLookup.remember(
            dustSlots(dusts), static_cast<size_t>(dusts->length), index, d);
    // O ChildSafety pode ter trocado o tipo (16) ou desligado a poeira.
    const int mi = d && activeOf(d) ? modIndex(typeOf(d)) : -1;
    if (mi < 0) return index;
    Rect& frame = field<Rect>(d, refs().frame);
    frame.x = 0;
    frame.y %= 30;
    if (g_hooks[mi] & kDustOnSpawn) {
        if (csharp::hasManagedDustType(typeOf(d)))
            managedDustPhase(d, 0, nullptr);
        else if (g_cb.spawn)
            g_cb.spawn(typeOf(d), d);
    }
    return index;
}

void hkUpdateDust(const MethodInfo* m) {
    Il2CppArray* dusts = readStatic<Il2CppArray*>(refs().mainDust);
    if (!dusts || dusts->length < static_cast<uintptr_t>(kDustSlots)) return g_origUpdateDust(m);

    const bool mods = g_installed.load(std::memory_order_relaxed) > 0;
    ++g_frame;
    if (!mods) {
        if (poolTopBroken() || g_frame % kIdleScanEvery == 0) {
            scanDusts(dusts, 0, g_scan, false);
            checkPool(dusts, g_scan.active);
        }
        g_origUpdateDust(m);
        sweepManagedDustLifecycle(dusts);
        return;
    }

    Timed timed(g_updateNs);
    ++g_updates;
    scanDusts(dusts, scanLimit(), g_scan, true);
    checkPool(dusts, g_scan.active);
    if (g_scan.mods.empty()) {
        g_origUpdateDust(m);
        sweepManagedDustLifecycle(dusts);
        return;
    }

    // ModDust.Update: false tira a poeira do laco do jogo neste quadro.
    g_list.clear();
    if (g_anyHooks & kDustUpdate) {
        for (Il2CppObject* d : g_scan.mods) {
            if ((g_hooks[modIndex(typeOf(d))] & kDustUpdate) &&
                !csharp::hasManagedDustType(typeOf(d)))
                g_list.push_back(d);
        }
    }
    g_flags.assign(g_list.size(), true);
    if (!g_list.empty() && g_cb.update) {
        beginKillTracking();
        g_cb.update(g_list, g_flags);
        endKillTracking(g_list);
    }

    g_hidden.clear();
    g_swapped.clear();
    size_t next = 0;
    for (Il2CppObject* d : g_scan.mods) {
        bool vanilla = true;
        if (next < g_list.size() && g_list[next] == d) vanilla = g_flags[next++];
        const int managedType = typeOf(d);
        const int managedMi = modIndex(managedType);
        if (managedMi >= 0 && (g_hooks[managedMi] & kDustOnSpawn) &&
            csharp::hasManagedDustType(managedType))
            managedDustPhase(d, 0, nullptr);
        if (managedMi >= 0 && (g_hooks[managedMi] & kDustUpdate) &&
            csharp::hasManagedDustType(managedType)) {
            std::uint32_t response = 0;
            if (managedDustPhase(d, 1, &response) && (response & 2u) == 0u)
                vanilla = false;
        }
        if (!activeOf(d)) continue;                        // o Update a apagou
        const int mi = modIndex(typeOf(d));
        if (mi < 0) continue;                              // o Update trocou o tipo
        if (!vanilla) {
            activeOf(d) = false;
            g_hidden.push_back(d);
            continue;
        }
        const int updateType = g_updateType[mi];
        if (updateType >= 0) {
            g_swapped.push_back({d, typeOf(d), updateType});
            typeOf(d) = updateType;
        }
    }

    g_origUpdateDust(m);

    for (Il2CppObject* d : g_hidden) activeOf(d) = true;
    for (const Swapped& s : g_swapped) {
        if (typeOf(s.dust) == s.updateType) typeOf(s.dust) = s.realType;
    }

    // ModDust.MidUpdate: depois do laco do jogo (o tModLoader o chama no meio).
    if (g_anyHooks & kDustMidUpdate) {
        g_list.clear();
        for (Il2CppObject* d : g_scan.mods) {
            if (!activeOf(d)) continue;
            const int mi = modIndex(typeOf(d));
            if (mi >= 0 && (g_hooks[mi] & kDustMidUpdate) &&
                !csharp::hasManagedDustType(typeOf(d))) g_list.push_back(d);
        }
        g_flags.assign(g_list.size(), false);
        if (!g_list.empty() && g_cb.midUpdate) {
            beginKillTracking();
            g_cb.midUpdate(g_list, g_flags);
            endKillTracking(g_list);
        }
        for (size_t i = 0; i < g_list.size(); ++i) {
            Il2CppObject* d = g_list[i];
            const int mi = modIndex(typeOf(d));
            if (g_flags[i] && activeOf(d) && mi >= 0 && g_updateType[mi] < 0)
                field<Vec2>(d, refs().velocity).x /= 0.99f;
        }
        for (Il2CppObject* d : g_scan.mods) {
            if (!activeOf(d)) continue;
            const int type = typeOf(d);
            const int mi = modIndex(type);
            if (mi < 0 || (g_hooks[mi] & kDustMidUpdate) == 0 ||
                !csharp::hasManagedDustType(type)) continue;
            std::uint32_t flags = 0;
            if (managedDustPhase(d, 2, &flags) && (flags & 4u) != 0u &&
                activeOf(d) && g_updateType[mi] < 0)
                field<Vec2>(d, refs().velocity).x /= 0.99f;
        }
    }
    sweepManagedDustLifecycle(dusts);
}

void swapTexture(Il2CppObject* asset) {
    il2cpp::api().field_static_set_value(refs().texture, asset);
}

void hkDrawDust(Il2CppObject* self, const MethodInfo* m) {
    Il2CppArray* dusts = g_installed.load(std::memory_order_relaxed) > 0
                             ? readStatic<Il2CppArray*>(refs().mainDust) : nullptr;
    if (!dusts || dusts->length < static_cast<uintptr_t>(kDustSlots)) return g_origDrawDust(self, m);

    Timed timed(g_drawNs);
    ++g_draws;
    Scan& scan = g_scan;
    scanDusts(dusts, scanLimit(), scan, true);
    if (scan.mods.empty()) return g_origDrawDust(self, m);

    // ModDust.PreDraw, antes de tudo (o rastro fica por baixo). O lote e do JS.
    std::vector<Il2CppObject*> skip;
    if (g_anyHooks & kDustPreDraw) {
        for (Il2CppObject* d : scan.mods) {
            if (!activeOf(d)) continue;
            const int type = typeOf(d);
            const int mi = modIndex(type);
            if (mi < 0 || (g_hooks[mi] & kDustPreDraw) == 0 ||
                !csharp::hasManagedDustType(type)) continue;
            std::uint32_t flags = 0;
            if (managedDustPhase(d, 4, &flags) && (flags & 16u) == 0u)
                skip.push_back(d);
        }
        g_list.clear();
        for (Il2CppObject* d : scan.mods) {
            if ((g_hooks[modIndex(typeOf(d))] & kDustPreDraw) &&
                !csharp::hasManagedDustType(typeOf(d))) g_list.push_back(d);
        }
        g_flags.assign(g_list.size(), true);
        if (!g_list.empty() && g_cb.preDraw) {
            beginKillTracking();
            g_cb.preDraw(g_list, g_flags);
            endKillTracking(g_list);
        }
        for (size_t i = 0; i < g_list.size(); ++i) {
            if (!g_flags[i]) skip.push_back(g_list[i]);
        }
    }
    auto skipped = [&skip](Il2CppObject* d) {
        for (Il2CppObject* s : skip) if (s == d) return true;
        return false;
    };

    // 1a passada: as do jogo e as de mod na folha do jogo.
    g_hidden.clear();
    size_t groups = 0;
    for (Il2CppObject* d : scan.mods) {
        if (!activeOf(d)) continue;
        const int mi = modIndex(typeOf(d));
        if (mi < 0) continue;
        if (g_group[mi] != 0 || skipped(d)) {
            activeOf(d) = false;
            g_hidden.push_back(d);
            if (g_group[mi] > static_cast<int>(groups)) groups = static_cast<size_t>(g_group[mi]);
        }
    }
    g_origDrawDust(self, m);
    for (Il2CppObject* d : g_hidden) activeOf(d) = true;

    // Uma passada por textura propria: so as poeiras dela ficam ativas, com a
    // textura dela no lugar da folha do jogo. O jogo desenha, ilumina e apaga
    // (cor preta) cada uma como faria com as dele.
    if (groups == 0) return;
    Il2CppObject* sheet = readStatic<Il2CppObject*>(refs().texture);
    for (size_t g = 1; g <= groups; ++g) {
        Il2CppObject* asset = il2cpp::api().gchandle_get_target(g_groupAssets[g - 1]);
        if (!asset) continue;
        g_hidden.clear();
        bool any = false;
        for (Il2CppObject* d : scan.all) {
            if (!activeOf(d)) continue;
            const int mi = modIndex(typeOf(d));
            const bool mine = mi >= 0 && g_group[mi] == static_cast<int>(g) && !skipped(d);
            if (mine) { any = true; ++g_groupDusts; continue; }
            activeOf(d) = false;
            g_hidden.push_back(d);
        }
        if (any) {
            ++g_groupPasses;
            swapTexture(asset);
            g_origDrawDust(self, m);
            swapTexture(sheet);
        }
        for (Il2CppObject* d : g_hidden) activeOf(d) = true;
    }
}

bool wantsAlpha(Il2CppObject* self) {
    if (field<bool>(self, refs().fullBright)) return false;
    const int mi = modIndex(typeOf(self));
    return mi >= 0 && (g_hooks[mi] & kDustGetAlpha) &&
           (g_cb.getAlpha || csharp::hasManagedDustType(typeOf(self)));
}

void hkGetAlphaRef(Il2CppObject* self, Color32* color, const MethodInfo* m) {
    if (color && wantsAlpha(self)) {
        uint32_t c = color->packed;
        if (csharp::hasManagedDustType(typeOf(self))) {
            uint32_t flags = 0;
            if (managedDustPhase(self, 3, &flags, &c) && (flags & 8u)) {
                color->packed = c;
                return;
            }
        } else if (g_cb.getAlpha && g_cb.getAlpha(self, c)) {
            color->packed = c;
            return;
        }
    }
    g_origGetAlphaRef(self, color, m);
}

Color32 hkGetAlphaValue(Il2CppObject* self, Color32 color, const MethodInfo* m) {
    if (wantsAlpha(self)) {
        uint32_t c = color.packed;
        if (csharp::hasManagedDustType(typeOf(self))) {
            uint32_t flags = 0;
            if (managedDustPhase(self, 3, &flags, &c) && (flags & 8u))
                return Color32{c};
        } else if (g_cb.getAlpha && g_cb.getAlpha(self, c))
            return Color32{c};
    }
    return g_origGetAlphaValue(self, color, m);
}

bool g_poolHooked = false;

void installPoolHook() {
    if (g_poolHooked) return;
    g_poolHooked = true;
    if (!hook::install(refs().updateDust, hkUpdateDust, &g_origUpdateDust)) {
        BL_ERROR("poeiras: sem hook no Dust.UpdateDust; a pilha de poeiras livres nao e conferida");
    }
}

bool installDustAlphaHooks() {
    if ((g_anyHooks & kDustGetAlpha) == 0) return true;
    const Refs& r = refs();
    if (!g_alphaRefHooked)
        g_alphaRefHooked = hook::install(r.getAlphaRef, hkGetAlphaRef, &g_origGetAlphaRef);
    if (!g_alphaValueHooked)
        g_alphaValueHooked = hook::install(r.getAlphaValue, hkGetAlphaValue, &g_origGetAlphaValue);
    return g_alphaRefHooked && g_alphaValueHooked;
}

bool installModHooks() {
    const Refs& r = refs();
    bool ok = hook::install(r.newDust, hkNewDust, &g_origNewDust);
    ok &= hook::install(r.drawDust, hkDrawDust, &g_origDrawDust);
    ok &= installDustAlphaHooks();
    if (!ok) BL_ERROR("poeiras de mod: hooks do Dust/Main.DrawDust faltando");
    return ok;
}

bool gameReady() {
    const Refs& r = refs();
    if (!r.ok) return false;
    Il2CppArray* dusts = readStatic<Il2CppArray*>(r.mainDust);
    return dusts && readStatic<Il2CppArray*>(r.freeDust) && readStatic<Il2CppArray*>(r.staticAlpha) &&
           readStatic<Il2CppObject*>(r.texture);
}

} // namespace

int registerModDust(ModDustDef def) {
    std::lock_guard<std::mutex> l(g_mx);
    for (const Entry& e : g_regs) {
        if (e.def.mod == def.mod && e.def.name == def.name) return -1;
    }
    Entry e;
    e.def = std::move(def);
    g_regs.push_back(std::move(e));
    g_total.store(static_cast<int>(g_regs.size()), std::memory_order_release);
    return kVanillaDustCount + static_cast<int>(g_regs.size()) - 1;
}

bool isModDust(int type) {
    return type >= kVanillaDustCount && type < kVanillaDustCount + g_total.load(std::memory_order_acquire);
}

int modDustTypeByName(const std::string& mod, const std::string& name) {
    std::lock_guard<std::mutex> l(g_mx);
    for (size_t i = 0; i < g_regs.size(); ++i) {
        if (g_regs[i].def.mod == mod && g_regs[i].def.name == name) return kVanillaDustCount + static_cast<int>(i);
    }
    return -1;
}

bool setModDustManagedHooks(int type, unsigned hooks) {
    if ((hooks & ~31u) != 0u) return false;
    const int index = type - kVanillaDustCount;
    std::lock_guard<std::mutex> lock(g_mx);
    if (index < 0 || index >= static_cast<int>(g_regs.size())) return false;
    g_regs[static_cast<size_t>(index)].def.hooks = hooks;
    g_maskDirty.store(true, std::memory_order_release);
    return true;
}

void setModDustUpdateType(int type, int updateType) {
    const int i = type - kVanillaDustCount;
    std::lock_guard<std::mutex> l(g_mx);
    if (i < 0 || i >= static_cast<int>(g_regs.size())) return;
    g_regs[static_cast<size_t>(i)].updateType = updateType;
    if (i < static_cast<int>(g_updateType.size())) g_updateType[static_cast<size_t>(i)] = updateType;
}

Il2CppObject* modDustTextureAsset(int type) {
    const int i = type - kVanillaDustCount;
    if (i < 0 || i >= g_installed.load(std::memory_order_acquire)) return nullptr;
    const int group = g_group[static_cast<size_t>(i)];
    if (group == 0) return readStatic<Il2CppObject*>(refs().texture);
    return il2cpp::api().gchandle_get_target(g_groupAssets[static_cast<size_t>(group - 1)]);
}

void setDustCallbacks(const DustCallbacks& callbacks) {
    g_cb = callbacks;
}

void tickModDusts() {
    if (g_failed) return;
    if (!gameReady()) return;
    installPoolHook();

    const int total = g_total.load(std::memory_order_acquire);
    const int installed = g_installed.load(std::memory_order_relaxed);
    const int from = kVanillaDustCount + installed;
    if (installed == total) {
        if (total > 0) {
            if (g_maskDirty.exchange(false, std::memory_order_acq_rel)) {
                {
                    std::lock_guard<std::mutex> lock(g_mx);
                    for (int i = 0; i < total; ++i)
                        g_hooks[static_cast<size_t>(i)] =
                            g_regs[static_cast<size_t>(i)].def.hooks;
                    g_anyHooks = 0;
                    for (unsigned mask : g_hooks) g_anyHooks |= mask;
                }
                if (!installDustAlphaHooks())
                    BL_ERROR("[CSharp][DustLoader] Stage=AlphaHooks Reason=HookInstallFailed");
            }
            g_tables.checkPending(from);
            static int frame = 0;
            if (++frame % 120 == 0) g_tables.watch(from, nullptr);
        }
        return;
    }
    if (installed > 0) {
        BL_ERROR("poeiras de mod: %d registrada(s) depois da instalacao; ficam sem tabela", total - installed);
        g_failed = true;
        return;
    }

    const int vanilla = readStatic<int16_t>(refs().dustCount);
    if (vanilla != kVanillaDustCount) {
        BL_ERROR("poeiras de mod: DustID.Count e %d, esperado %d; poeiras de mod desligadas", vanilla,
                 kVanillaDustCount);
        g_failed = true;
        return;
    }
    const int to = kVanillaDustCount + total;
    const int grown = g_tables.grow(from, to);
    if (grown < 0) {
        BL_ERROR("poeiras de mod: nenhuma tabela de poeira achada; poeiras de mod desligadas");
        g_failed = true;
        return;
    }

    {
        std::lock_guard<std::mutex> l(g_mx);
        g_hooks.assign(static_cast<size_t>(total), 0);
        g_updateType.assign(static_cast<size_t>(total), -1);
        g_group.assign(static_cast<size_t>(total), 0);
        std::vector<std::string> paths;
        for (int i = 0; i < total; ++i) {
            Entry& e = g_regs[static_cast<size_t>(i)];
            g_hooks[static_cast<size_t>(i)] = e.def.hooks;
            g_updateType[static_cast<size_t>(i)] = e.updateType;
            g_anyHooks |= e.def.hooks;
            if (e.def.texture.empty()) continue;
            // Tipos com o mesmo PNG dividem a passada de desenho.
            size_t k = 0;
            while (k < paths.size() && paths[k] != e.def.texture) ++k;
            if (k == paths.size()) {
                int w = 0, h = 0;
                Il2CppObject* asset = content::loadTextureAsset(e.def.texture, nullptr, 0,
                                                                e.def.mod + "/" + e.def.name, &w, &h);
                if (!asset) {
                    BL_ERROR("poeira de mod %s: textura nao carregou (%s); usa a folha do jogo",
                             e.def.name.c_str(), e.def.texture.c_str());
                    continue;
                }
                paths.push_back(e.def.texture);
                g_groupAssets.push_back(il2cpp::api().gchandle_new(asset, false));
                BL_DEBUG("poeira de mod %s: textura %dx%d, Value %p", e.def.name.c_str(), w, h,
                         (void*)content::textureAssetValue(asset));
            }
            e.asset = g_groupAssets[k];
            g_group[static_cast<size_t>(i)] = static_cast<int>(k) + 1;
        }
    }
    g_installed.store(total, std::memory_order_release);
    installModHooks();
    BL_INFO("poeiras de mod: %d instalada(s) (tipos %d..%d), %d tabela(s) aumentadas, %zu textura(s)",
            total, from, to - 1, grown, g_groupAssets.size());
}

bool modDustsSettled() {
    return g_failed || g_installed.load(std::memory_order_relaxed) == g_total.load(std::memory_order_acquire);
}

DustPoolStats dustPoolStats() {
    DustPoolStats s;
    const Refs& r = refs();
    if (!r.ok) return s;
    Il2CppArray* dusts = readStatic<Il2CppArray*>(r.mainDust);
    if (!dusts || dusts->length < static_cast<uintptr_t>(kDustSlots)) return s;
    Il2CppObject** slots = dustSlots(dusts);
    for (int i = 0; i < kDustSlots; ++i) {
        Il2CppObject* d = slots[i];
        if (!d || !activeOf(d)) continue;
        ++s.active;
        if (modIndex(typeOf(d)) >= 0) ++s.modActive;
    }
    s.activeCount = readStatic<int32_t>(r.activeDustCount);
    s.freeCount = readStatic<int32_t>(r.freeDustCount);
    s.repairs = g_repairs.load();
    s.updateNs = g_updateNs;
    s.drawNs = g_drawNs;
    s.jsNs = g_jsNs;
    s.updates = g_updates;
    s.draws = g_draws;
    s.groupPasses = g_groupPasses;
    s.groupDusts = g_groupDusts;
    s.managedSyncAttempts = g_dustSyncAttempts;
    s.managedSyncReady = g_dustSyncReady;
    s.managedSyncMissed = g_dustSyncMissed;
    s.managedSyncWaitNs = g_dustSyncWaitNs;
    return s;
}

void resetManagedDustLifecycle() {
    g_dustLifecycle.reset();
    g_dustFrameBudget.reset();
    g_dustSyncAttempts = 0;
    g_dustSyncReady = 0;
    g_dustSyncMissed = 0;
    g_dustSyncWaitNs = 0;
    g_dustSlotLookup.reset();
    g_dustGenerations.fill(0);
    g_dustCached.fill({});
}

bool repairDustPool() {
    const Refs& r = refs();
    if (!r.ok) return false;
    Il2CppArray* dusts = readStatic<Il2CppArray*>(r.mainDust);
    if (!dusts || dusts->length < static_cast<uintptr_t>(kDustSlots)) return false;
    Scan scan;
    scanDusts(dusts, 0, scan, false);
    const int before = g_repairs.load();
    checkPool(dusts, scan.active);
    return g_repairs.load() != before;
}

} // namespace bl::runtime
