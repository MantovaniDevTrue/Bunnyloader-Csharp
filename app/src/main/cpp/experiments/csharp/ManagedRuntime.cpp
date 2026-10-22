#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "experiments/csharp/UniversalHooks.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeCommandMailbox.h"
#include "experiments/csharp/NativeCommandResult.h"
#include "experiments/csharp/ManagedLocalizationEpochFence.h"
#include "experiments/csharp/ManagedLiveSourceRevocations.h"
#include "experiments/csharp/ManagedDeferredModRevocation.h"
#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "experiments/csharp/ManagedDustMailbox.h"
#include "graphics/ScreenScene.h"
#include "experiments/csharp/ManagedVisualMonoPump.h"
#include "experiments/csharp/ManagedVisualState.h"
#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include "experiments/csharp/ManagedHitCallbackDispatch.h"
#include "experiments/csharp/ManagedProcessMemory.h"
#include "experiments/csharp/MonoSignalOwnership.h"
#include "experiments/csharp/NativeAbortProbe.h"
#include "experiments/csharp/CompilerAot.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <array>
#include <deque>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <cstring>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <dlfcn.h>
#include <mutex>
#include <condition_variable>
#include <pthread.h>
#include <dirent.h>
#include <chrono>
#include <cstdlib>
#include <sys/syscall.h>
#include <map>
#include <memory>
#include <set>
#include <limits>
#include <thread>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bl::csharp {
struct LiveCallbackResult {
    bool ready = false;
    int value = 0;
    std::string phase;
    std::string error;
    bool invoked = false;
    bool usedV2 = false;
    bool usedV3 = false;
    bool usedV4 = false;
};
LiveCallbackResult invokeLiveSourceEvent(const std::string& modId,
                                         const std::string& assemblyPath,
                                         bool isKill, int tick, int damage, int npcIndex,
                                         const ManagedNpcSnapshot& npc,
                                         const ManagedPlayerSnapshot& player,
                                         const std::string& callbackClass = "BunnyLiveEvents");
ManagedRuntimeStatus probeManagedRuntimeStartupStage(
    const std::string& assemblyPath, const std::string& stage);

namespace {

#ifdef __ANDROID__
void reportManagedItemUse(int level, const std::string& message) {
    using LogBridge = void (*)(int, const char*);
    void* bunny = dlopen("libbunny.so", RTLD_NOW | RTLD_NOLOAD);
    auto forward = bunny
        ? reinterpret_cast<LogBridge>(dlsym(bunny, "bunny_native_forward_log"))
        : nullptr;
    if (forward) forward(level, message.c_str());
    else __android_log_write(level, "BunnyCSharp", message.c_str());
    if (bunny) dlclose(bunny);
}
#endif

using InitFn = void* (*)(const char*);
using AssemblyPathFn = void (*)(const char*);
using AttachFn = void* (*)(void*);
using CurrentThreadFn = void* (*)();
using DetachFn = void (*)(void*);
using OpenFn = void* (*)(void*, const char*);
using ImageFn = void* (*)(void*);
using ClassFn = void* (*)(void*, const char*, const char*);
using MethodFn = void* (*)(void*, const char*, int);
using InvokeFn = void* (*)(void*, void*, void**, void**);
using UnboxFn = void* (*)(void*);
using NewStringFn = void* (*)(void*, const char*);
using MonoGcSizeFn = int64_t (*)();

struct MonoApi {
    InitFn initialize = nullptr;
    AssemblyPathFn setAssembliesPath = nullptr;
    AttachFn attach = nullptr;
    CurrentThreadFn currentThread = nullptr;
    DetachFn detachThread = nullptr;
    OpenFn openAssembly = nullptr;
    ImageFn image = nullptr;
    ClassFn findClass = nullptr;
    MethodFn findMethod = nullptr;
    InvokeFn invoke = nullptr;
    UnboxFn unbox = nullptr;
    NewStringFn newString = nullptr;
    MonoGcSizeFn gcHeapSize = nullptr;
    MonoGcSizeFn gcUsedSize = nullptr;
};

std::mutex g_mutex;
ManagedRuntimeStatus g_status;
MonoApi g_api;
void* g_library = nullptr;
void* g_domain = nullptr;
CompilerAotResult g_compilerAot;
bool g_compilerAotAttempted = false;
long long g_compilerAotSetupMs = 0;
std::string g_compilerMethodCode = "NotChecked";
std::string g_checkpointPath;
void* g_liveAiMethod = nullptr;

struct LiveEntry {
    std::string path;
    void* method = nullptr;
    void* npcAiMethod = nullptr;
    void* hitMethod = nullptr;
    void* hitV2Method = nullptr;
    void* hitV3Method = nullptr;
    void* hitV4Method = nullptr;
    bool hitMethodsResolved = false;
    void* killMethod = nullptr;
    LiveSourceStatus status;
};

std::map<std::string, LiveEntry> g_sourceLiveEntries;
ManagedLiveSourceRevocations g_liveSourceRevocations;
uint64_t g_liveMethodCacheHits = 0;
uint64_t g_liveMethodCacheMisses = 0;
uint64_t g_liveAssemblyOpenAttempts = 0;
uint64_t g_liveAssemblyOpenSuccesses = 0;
std::set<std::string> g_liveAssemblyOpenPaths;
std::mutex g_memoryBaselineMutex;
int64_t g_memoryBaselineRssKb = 0;

template <typename T>
T resolve(const char* name) {
    return reinterpret_cast<T>(dlsym(g_library, name));
}

void saveCheckpoint() {
    if (g_checkpointPath.empty()) return;
    const std::string tmp = g_checkpointPath + ".tmp";
    FILE* file = std::fopen(tmp.c_str(), "wb");
    if (!file) return;
    std::fprintf(file, "timestampUnix=%lld\npid=%ld\nphase=%s\n"
                       "attempted=%d\nlibraryLoaded=%d\nsymbolsReady=%d\n"
                       "domainInitialized=%d\nmethodExecuted=%d\nresult=%d\nerror=%s\n",
                 static_cast<long long>(std::time(nullptr)), static_cast<long>(getpid()),
                 g_status.phase.c_str(), g_status.attempted,
                 g_status.libraryLoaded, g_status.symbolsReady,
                 g_status.domainInitialized, g_status.methodExecuted,
                 g_status.result, g_status.error.c_str());
    const bool flushed = std::fflush(file) == 0;
    if (flushed) fsync(fileno(file));
    std::fclose(file);
    if (flushed) std::rename(tmp.c_str(), g_checkpointPath.c_str());
}

void trace(const char* phase, const char* detail = nullptr) {
    g_status.phase = phase;
    saveCheckpoint();
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                        "mono phase=%s detail=%s", phase, detail ? detail : "");
#else
    std::fprintf(stderr, "mono phase=%s detail=%s\n", phase, detail ? detail : "");
#endif
}

ManagedRuntimeStatus failure(const char* phase, const std::string& detail) {
    g_status.error = detail.substr(0, 512);
    g_status.methodExecuted = false;
    trace(phase, g_status.error.c_str());
    return g_status;
}

bool validAssembly(const std::string& path) {
    if (path.empty() || path.size() > 2048 || path.find("..") != std::string::npos)
        return false;
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           st.st_size > 0 && st.st_size <= 32 * 1024 * 1024;
}

bool loadLibrary() {
    if (g_library) return true;
    trace("opening-library");
    g_library = dlopen("libmonosgen-2.0.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_library) {
        const char* message = dlerror();
        failure("library-missing", message ? message : "libmonosgen-2.0.so indisponivel");
        return false;
    }
    g_status.libraryLoaded = true;
    trace("resolving-symbols");
    g_api.initialize = resolve<InitFn>("mono_jit_init");
    g_api.setAssembliesPath = resolve<AssemblyPathFn>("mono_set_assemblies_path");
    g_api.attach = resolve<AttachFn>("mono_thread_attach");
    g_api.currentThread = resolve<CurrentThreadFn>("mono_thread_current");
    g_api.detachThread = resolve<DetachFn>("mono_thread_detach");
    g_api.openAssembly = resolve<OpenFn>("mono_domain_assembly_open");
    g_api.image = resolve<ImageFn>("mono_assembly_get_image");
    g_api.findClass = resolve<ClassFn>("mono_class_from_name");
    g_api.findMethod = resolve<MethodFn>("mono_class_get_method_from_name");
    g_api.invoke = resolve<InvokeFn>("mono_runtime_invoke");
    g_api.unbox = resolve<UnboxFn>("mono_object_unbox");
    g_api.newString = resolve<NewStringFn>("mono_string_new");
    g_api.gcHeapSize = resolve<MonoGcSizeFn>("mono_gc_get_heap_size");
    g_api.gcUsedSize = resolve<MonoGcSizeFn>("mono_gc_get_used_size");
    if (!g_api.initialize || !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke || !g_api.unbox) {
        failure("abi-incompatible", "Mono Embedding API incompleta");
        dlclose(g_library);
        g_library = nullptr;
        g_api = {};
        g_status.libraryLoaded = false;
        return false;
    }
    g_status.symbolsReady = true;
    trace("symbols-ready");
    return true;
}



void recordDomainThreads(const char* phase, int seconds = -1, bool reset = false) {
    const auto slash = g_checkpointPath.find_last_of('/');
    if (slash == std::string::npos) return;
    const auto dir = g_checkpointPath.substr(0, slash + 1);
    std::string data = "unix=" + std::to_string(static_cast<long long>(std::time(nullptr))) +
        " pid=" + std::to_string(static_cast<long>(getpid())) +
        " tid=" + std::to_string(static_cast<long>(syscall(__NR_gettid))) +
        " phase=" + phase + " elapsed=" + std::to_string(seconds) + "\n";
    DIR* tasks = opendir("/proc/self/task");
    int count = 0;
    if (tasks) {
        while (dirent* entry = readdir(tasks)) {
            char* end = nullptr;
            const long tid = std::strtol(entry->d_name, &end, 10);
            if (tid <= 0 || !end || *end) continue;
            char location[100]{};
            std::snprintf(location, sizeof(location), "/proc/self/task/%ld/comm", tid);
            char name[100]{};
            if (FILE* source = std::fopen(location, "rb")) {
                if (std::fgets(name, sizeof(name), source))
                    name[std::strcspn(name, "\r\n")] = '\0';
                std::fclose(source);
            }
            data += "thread=" + std::to_string(tid) + " name=" + name + "\n";
            if (++count >= 256) break;
        }
        closedir(tasks);
    }
    data += "total=" + std::to_string(count) + "\nEND\n";
    for (int n = 0; n < 2; ++n) {
        const auto fileName = dir + (n ? "mono-domain-thread-audit-latest.txt" : "mono-domain-thread-audit.txt");
        FILE* out = std::fopen(fileName.c_str(), n == 0 && !reset ? "ab" : "wb");
        if (!out) continue;
        if (std::fwrite(data.data(), 1, data.size(), out) == data.size() &&
            std::fflush(out) == 0) fsync(fileno(out));
        std::fclose(out);
    }
}

constexpr int kAuditSamples[] = {2, 5, 15, 30, 60, 120};
bool g_auditEnabled = false;
size_t g_auditIndex = 0;
std::chrono::steady_clock::time_point g_auditStart;

struct UniversalMod { std::string id, path; bool registered = false; };
std::vector<UniversalMod> g_universalMods;
std::deque<std::string> g_universalPendingUnloads;
bool g_universalUnload = false;
bool g_universalEnabled = true;
void* g_universalRegister = nullptr;
void* g_universalDispatch = nullptr;
void* g_universalDispatchInt32 = nullptr;
void* g_universalDispatchInt32Pair = nullptr;
void* g_universalDispatchExtended = nullptr;
void* g_universalClear = nullptr;
void* g_universalUnloadMod = nullptr;
void* g_universalPauseWorld = nullptr;
void* g_universalRemove = nullptr;
void* g_universalError = nullptr;
void* g_universalSnapshot = nullptr;
std::atomic<bool> g_universalPolling{false};

std::mutex g_pinnedMutex;
std::condition_variable g_pinnedWake;
std::condition_variable g_pinnedResultWake;
struct RecipeRpcCompletion { bool done=false,cancelled=false;std::string output; };
struct RecipeRpcJob {
    std::string mod,assembly,owner,snapshot;int index=0,kind=0;
    std::chrono::steady_clock::time_point deadline;
    std::shared_ptr<RecipeRpcCompletion> completion;
    bool shooting=false;
    bool tile=false;
    bool rarity=false;
};
struct LocalizationJob {
    std::string modId;
    std::string assemblyPath;
    std::string culture;
    uint64_t generation = 0;
    bool unload = false;
    bool drain = false;
};
std::deque<LocalizationJob> g_localizationQueue;
std::deque<std::string> g_localizationUnloads;
std::map<std::string, LocalizationJob> g_localizationLiveBindings;
std::string g_lastLocalizationPollMod;
std::chrono::steady_clock::time_point g_nextLocalizationPoll{};
ManagedLocalizationEpochFence g_localizationEpochFence;
void* g_localizationDispatchMethod = nullptr;
void* g_localizationDrainMethod = nullptr;
void* g_localizationAckMethod = nullptr;
void* g_localizationUnloadMethod = nullptr;
std::atomic<ManagedLocalizationResultSink> g_localizationResultSink{nullptr};
std::deque<RecipeRpcJob> g_recipeRpcQueue;
std::condition_variable g_recipeRpcWake;
void* g_recipeDispatchMethod=nullptr;
void* g_shootDispatchMethod=nullptr;
void* g_tileDispatchMethod=nullptr;
void* g_rarityDispatchMethod=nullptr;
std::map<std::string,std::shared_ptr<RecipeRpcCompletion>> g_shootWarmups;
std::map<std::string,std::shared_ptr<RecipeRpcCompletion>> g_tileWarmups;
std::map<std::string,std::shared_ptr<RecipeRpcCompletion>> g_rarityWarmups;
std::atomic<bool> g_recipeRuntimeReady{false};

struct ManagedCommandBinding {
    std::string modId;
    std::string assembly;
    bool registered = false;
    int failures = 0;
};
std::vector<ManagedCommandBinding> g_commandBindings;
void* g_commandRegisterMethod = nullptr;
void* g_commandExecuteMethod = nullptr;
void* g_commandClearMethod = nullptr;
bool g_commandSessionReset = false;



struct SceneLightBinding {
    std::string modId, assemblyPath, nameSpace, className, methodName;
    std::chrono::steady_clock::time_point nextSample{};
    bool enabled = true;
    int failures = 0;
};
struct SceneLightRequest {
    int binding = -1;
    bl::graphics::ScreenSceneRaw raw;
    float baseSunX = 0, baseSunY = 0, baseClock = 0;
    uint64_t generation = 0;
};
struct SceneLightReply {
    ManagedScreenLightResult result;
    std::chrono::steady_clock::time_point updated{};
    uint64_t generation = 0;
};
std::set<std::string> g_revokedDeferredMods;
std::vector<SceneLightBinding> g_sceneLightBindings;
std::deque<SceneLightRequest> g_sceneLightQueue;
std::map<std::string, SceneLightReply> g_sceneLightResults;
uint64_t g_sceneLightGeneration = 1;
std::map<std::string, void*> g_sceneLightMethods;
struct ManagedHurtBinding {
    std::string modId;
    std::string assemblyPath;
    int managedSlot = -1;
    bool active = false;
    bool failed = false;
};
struct ManagedHurtCompletion {
    bool done = false;
    bool cancelled = false;
    bool expired = false;
    int64_t value = -1;
    std::chrono::steady_clock::time_point deadline{};
};
struct ManagedHurtJob {
    uint64_t ticket = 0;
    int modIndex = -1;
    bool after = false;
    int damage = 0;
    int sourceDamage = 0;
    int direction = 0;
    int flags = 0;
    int life = 0;
    int maxLife = 0;
    int equipmentMask = 0;
    int npcIndex = -1;
    int projectileIndex = -1;
    int npcType = -1;
    int npcDamage = -1;
    int npcLife = -1;
    int projectileType = -1;
    int projectileDamage = -1;
    int projectileOwner = -1;
    float attackerX = -1.f;
    float attackerY = -1.f;
    float attackerVelocityX = -1.f;
    float attackerVelocityY = -1.f;
    float attackerAngle = -1.f;
    float attackerDistance = -1.f;
    std::shared_ptr<ManagedHurtCompletion> completion{};
    uint64_t generation = 0;
};
std::vector<ManagedHurtBinding> g_managedHurtBindings;
std::deque<ManagedHurtJob> g_managedHurtQueue;
std::condition_variable g_managedHurtResultWake;
uint64_t g_managedHurtTicket = 0;
uint64_t g_managedHurtGeneration = 0;
ManagedHurtSchedulerStatus g_hurtScheduler;
struct ManagedEntityBinding {
    std::string modId;
    std::string assemblyPath;
    std::vector<std::pair<int, std::string>> npcTypes;
    std::vector<std::pair<int, std::string>> projectileTypes;
    int managedSlot = -1;
    bool armed = true;
    bool invasionSupported = true;
    bool firstWorldTickReported = false;
    bool firstWorldErrorReported = false;
    bool firstWorldUseReported = false;
    std::string lastUseOutcome;
    int useReports = 0;
};

struct ManagedEntityJob {
    int binding = -1;
    int kind = 0;
    int type = -1;
    int slot = -1;
    int generation = 0;
    int owner = -1;
    int life = 0;
    int lifeMax = 0;
    uintptr_t identity = 0;
    float x = 0, y = 0, vx = 0, vy = 0, playerX = 0, playerY = 0;
    uint64_t session = 0;
    int damage = 0;
    int target = -1;
    float targetX = 0.f;
    float targetY = 0.f;
};

struct ManagedEntityResult {
    int type = -1;
    int generation = 0;
    int owner = -1;
    uintptr_t identity = 0;
    int64_t packed = -1;
    bool ready = false;
};

struct ManagedDustBinding {
    std::string modId;
    std::string assemblyPath;
    std::vector<std::pair<int, std::string>> types;
    int managedSlot = -1;
    bool enabled = true;
};
ManagedDustMailbox g_dustMailbox;
std::atomic<uint64_t> g_dustGameFrame{0};
std::vector<ManagedDustBinding> g_dustBindings;
std::deque<int> g_dustBootstrap;
std::array<std::atomic<int>, 32768> g_dustOwners{};
std::array<std::atomic<bool>, 16> g_dustReady{};
std::atomic<int> g_dustModCount{0};
void* g_dustRegisterMethod = nullptr;
void* g_dustBindMethod = nullptr;
void* g_dustInvokeMethod = nullptr;
void* g_dustResetMethod = nullptr;
bool g_dustWorldResetPending = false;

std::vector<ManagedEntityBinding> g_entityBindings;
std::deque<ManagedEntityJob> g_entityQueue;
std::array<ManagedEntityResult, 512> g_entityNpcResults{};
std::array<ManagedEntityResult, 4096> g_entityProjectileResults{};
std::array<bool, 512> g_entityNpcPending{};
std::array<bool, 4096> g_entityProjectilePending{};
std::array<unsigned, 512> g_entityNpcTicks{};
std::array<unsigned, 4096> g_entityProjectileTicks{};
std::array<std::atomic<int>, 32768> g_entityNpcTypeOwner{};
std::array<std::atomic<int>, 32768> g_entityProjectileTypeOwner{};
std::atomic<int> g_entityModCount{0};
uint64_t g_entitySession = 1;
void* g_entityRegister = nullptr;
void* g_entityBind = nullptr;
void* g_entityNpcTick = nullptr;
void* g_entityWorldTick = nullptr;
void* g_entityStatusMethod = nullptr;
void* g_entityProjectileTick = nullptr;
void* g_entityNpcHit = nullptr;
void* g_entityNpcKill = nullptr;
void* g_entityProjectileHit = nullptr;
void* g_entityProjectileKill = nullptr;
std::atomic<int> g_entityCombatSubmitted{0};
std::atomic<int> g_entityCombatDropped{0};
std::array<std::atomic<int>, 4> g_entityCombatExecuted{};
std::array<std::atomic<int>, 4> g_entityCombatQueued{};
std::array<std::atomic<int>, 4> g_entityCombatUnbound{};
std::array<std::atomic<int>, 4> g_entityCombatRejected{};
std::array<std::atomic<int>, 4> g_entityCombatFaulted{};
std::array<std::atomic<int>, 4> g_entityCombatLost{};
std::array<std::atomic<int>, 4> g_entityCombatDroppedByKind{};
std::atomic<int> g_entityCombatStatusWrites{0};
std::array<std::atomic<int>, 2> g_entityAiProcessed{};


std::atomic<int> g_entityJobsSubmitted{0};
std::atomic<int> g_entityJobsCompleted{0};
std::atomic<int> g_entityJobsDropped{0};

void* g_managedHurtRegisterMethod = nullptr;
void* g_managedHurtModifyMethod = nullptr;
void* g_managedHurtAfterMethod = nullptr;
void* g_managedHurtErrorMethod = nullptr;
void* g_managedHurtSnapshotMethod = nullptr;
std::mutex g_hurtSnapshotMutex;
std::string g_hurtSnapshotText = "Hooks C#: aguardando evento de dano...";
std::chrono::steady_clock::time_point g_hurtSnapshotNext{};
std::atomic<ManagedHurtDiagnosticLogFn> g_hurtDiagnosticLogger{nullptr};

bool g_pinnedStarted = false;
bool g_pinnedBusy = false;
bool g_pinnedPending = false;
bool g_pinnedResultReady = false;
long g_pinnedTid = 0;
std::string g_pinnedAssembly;
std::string g_pinnedStage = "PINNED_MONO";
ManagedRuntimeStatus g_pinnedResult;
ManagedLiveHitStatus g_liveHitStatus;
int g_primaryLiveFailures = 0;
std::atomic<int> g_liveHitType{-1};
std::atomic<bool> g_liveHitAutomatic{false};
std::atomic<int> g_liveHitLockDrops{0};
ManagedLiveEventQueue g_liveHitQueue;
ManagedLiveIngressQueue g_liveIngress;
bool g_liveHitInFlight = false;
int g_liveHitActiveBindingSlot = -2;
uint64_t g_liveHitGeneration = 0;
std::atomic<uint64_t> g_buffEffectGeneration{1};
std::array<std::atomic<uint64_t>, ManagedLiveIngressQueue::kBindingCount>
    g_buffOwnerGenerations{};
std::atomic<bool> g_buffHookReady{false};
ManagedNpcBuffEffectQueue g_buffEffectQueue;
std::atomic<int> g_buffRequested{0};
std::atomic<int> g_buffApplied{0};
std::atomic<int> g_buffRejected{0};
std::atomic<int> g_buffStale{0};
std::atomic<int> g_buffQueueDrops{0};
std::atomic<int> g_buffLastId{0};
std::atomic<int> g_buffLastDuration{0};
std::atomic<int> g_buffLastNpc{-1};
std::atomic<int> g_impulsesApplied{0};
std::atomic<int> g_lastImpulseDirection{0};
std::atomic<int> g_lastImpulseTenths{0};
std::string g_liveHitAssembly;

struct AdditionalLiveBinding {
    std::string modId;
    std::string projectile;
    std::string callbackClass;
    std::string assemblyPath;
    int type = -1;
    std::atomic<int> armedType{-1};
    std::atomic<int> queued{0};
    int processed = 0;
    int hits = 0;
    int kills = 0;
    int failures = 0;
    int dropped = 0;
    int lastResult = 0;
    ManagedHitContextStats hitContext;
    std::string error;
};

std::array<AdditionalLiveBinding, ManagedLiveIngressQueue::kBindingCount - 1> g_additionalLiveBindings{};
std::array<std::atomic<int16_t>, 32768> g_additionalLiveByType{};
std::atomic<int> g_additionalLiveCount{0};

void disarmAdditionalLiveBindings() {
    const int count = g_additionalLiveCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
        g_additionalLiveBindings[static_cast<size_t>(i)].armedType.store(
            -1, std::memory_order_release);
}

VmCompareStatus g_vmCompare;
bool g_vmComparePending = false;
int g_vmCompareRuns = 0;
std::atomic<int> g_perfState{0};
std::atomic<int64_t> g_perfStartNs{0};
std::atomic<int64_t> g_perfEndNs{0};
std::atomic<int64_t> g_perfPreviousUpdateNs{0};
std::atomic<int64_t> g_perfUpdateTotalUs{0};
std::atomic<int> g_perfUpdateSamples{0};
std::atomic<int> g_perfOver25{0};
std::atomic<int> g_perfOver33{0};
std::array<std::atomic<uint32_t>, 26> g_perfUpdateBins{};
std::atomic<int64_t> g_perfQueueTotalUs{0};
std::atomic<int64_t> g_perfCallbackTotalUs{0};
std::atomic<int> g_perfQueueMaxUs{0};
std::atomic<int> g_perfCallbackMaxUs{0};
std::atomic<int> g_perfCallbacks{0};
std::atomic<int> g_perfLockDropBaseline{0};
std::atomic<int> g_perfQueueSamples{0};
std::atomic<int64_t> g_perfCallbackCpuTotalUs{0};
std::atomic<int> g_perfCallbackCpuMaxUs{0};
std::atomic<int> g_perfAccepted{0};
std::atomic<int> g_perfProcessed{0};
std::atomic<int> g_perfFailed{0};
std::atomic<int> g_perfQueueDropped{0};
std::atomic<int> g_perfMaxQueueDepth{0};
std::atomic<int> g_perfProjectileCurveUpdates{0};
std::atomic<int> g_perfProjectileVelocityWrites{0};
using PerfUsBuckets = std::array<std::atomic<uint32_t>, 20>;
PerfUsBuckets g_perfQueueBins{};
PerfUsBuckets g_perfCallbackBins{};
PerfUsBuckets g_perfCallbackCpuBins{};
constexpr std::array<int, 20> kPerfUsThresholds{{
    25, 50, 75, 100, 150, 200, 250, 350, 500, 700,
    1000, 1500, 2250, 3500, 5000, 7500, 12000, 20000, 50000, 2147483647
}};
struct PerfProjectileCounters {
    std::atomic<int> callbacks{0};
    std::atomic<int> hits{0};
    std::atomic<int> kills{0};
    std::atomic<int> failures{0};
    std::atomic<int> queueSamples{0};
    std::atomic<int64_t> queueTotalUs{0};
    std::atomic<int> queueMaxUs{0};
    std::atomic<int64_t> callbackTotalUs{0};
    std::atomic<int> callbackMaxUs{0};
    std::atomic<int64_t> cpuTotalUs{0};
    std::atomic<int> cpuMaxUs{0};
    PerfUsBuckets callbackBins{};
};
std::array<PerfProjectileCounters, ManagedLiveIngressQueue::kBindingCount> g_perfProjectileCounters{};
constexpr int64_t kPerfDurationNs = 15000000000LL;

int64_t perfNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void updatePerfMax(std::atomic<int>& high, int value) {
    int previous = high.load(std::memory_order_relaxed);
    while (value > previous && !high.compare_exchange_weak(
        previous, value, std::memory_order_relaxed)) {}
}

int64_t perfThreadCpuNs() {
    timespec time{};
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0
        ? static_cast<int64_t>(time.tv_sec) * 1000000000LL + time.tv_nsec : 0;
}

void perfRecordUs(PerfUsBuckets& histogram, int64_t us) {
    if (us < 0) return;
    for (size_t i = 0; i < kPerfUsThresholds.size(); ++i) {
        if (us <= kPerfUsThresholds[i]) {
            histogram[i].fetch_add(1, std::memory_order_relaxed);
            break;
        }
    }
}

int perfPercentileUs(const PerfUsBuckets& histogram, int observations, int percentile) {
    if (observations <= 0) return 0;
    const int target = (observations * percentile + 99) / 100;
    int cumulative = 0;
    for (size_t i = 0; i < kPerfUsThresholds.size(); ++i) {
        cumulative += static_cast<int>(histogram[i].load(std::memory_order_relaxed));
        if (cumulative >= target) return kPerfUsThresholds[i];
    }
    return 0;
}

void resetPerfCounters() {
    g_perfStartNs.store(0, std::memory_order_relaxed);
    g_perfEndNs.store(0, std::memory_order_relaxed);
    g_perfPreviousUpdateNs.store(0, std::memory_order_relaxed);
    g_perfUpdateTotalUs.store(0, std::memory_order_relaxed);
    g_perfUpdateSamples.store(0, std::memory_order_relaxed);
    g_perfOver25.store(0, std::memory_order_relaxed);
    g_perfOver33.store(0, std::memory_order_relaxed);
    for (auto& bucket : g_perfUpdateBins)
        bucket.store(0, std::memory_order_relaxed);
    g_perfQueueTotalUs.store(0, std::memory_order_relaxed);
    g_perfCallbackTotalUs.store(0, std::memory_order_relaxed);
    g_perfQueueMaxUs.store(0, std::memory_order_relaxed);
    g_perfCallbackMaxUs.store(0, std::memory_order_relaxed);
    g_perfCallbacks.store(0, std::memory_order_relaxed);
    g_perfQueueSamples.store(0, std::memory_order_relaxed);
    g_perfCallbackCpuTotalUs.store(0, std::memory_order_relaxed);
    g_perfCallbackCpuMaxUs.store(0, std::memory_order_relaxed);
    g_perfAccepted.store(0, std::memory_order_relaxed);
    g_perfProcessed.store(0, std::memory_order_relaxed);
    g_perfFailed.store(0, std::memory_order_relaxed);
    g_perfQueueDropped.store(0, std::memory_order_relaxed);
    g_perfMaxQueueDepth.store(0, std::memory_order_relaxed);
    g_perfProjectileCurveUpdates.store(0, std::memory_order_relaxed);
    g_perfProjectileVelocityWrites.store(0, std::memory_order_relaxed);
    for (auto& histogram : {&g_perfQueueBins, &g_perfCallbackBins, &g_perfCallbackCpuBins})
        for (auto& bucket : *histogram)
            bucket.store(0, std::memory_order_relaxed);
    for (auto& p : g_perfProjectileCounters) {
        p.callbacks.store(0, std::memory_order_relaxed);
        p.hits.store(0, std::memory_order_relaxed);
        p.kills.store(0, std::memory_order_relaxed);
        p.failures.store(0, std::memory_order_relaxed);
        p.queueSamples.store(0, std::memory_order_relaxed);
        p.queueTotalUs.store(0, std::memory_order_relaxed);
        p.queueMaxUs.store(0, std::memory_order_relaxed);
        p.callbackTotalUs.store(0, std::memory_order_relaxed);
        p.callbackMaxUs.store(0, std::memory_order_relaxed);
        p.cpuTotalUs.store(0, std::memory_order_relaxed);
        p.cpuMaxUs.store(0, std::memory_order_relaxed);
        for (auto& bucket : p.callbackBins)
            bucket.store(0, std::memory_order_relaxed);
    }
}

ManagedLiveBenchmarkStatus g_liveBenchmark;
bool g_liveBenchmarkAttempted = false;
std::chrono::steady_clock::time_point g_liveBenchmarkStart{};
std::chrono::steady_clock::time_point g_liveBenchmarkEnd{};

constexpr const char* kBenchmarkModId = "68c3115f-62f4-49f2-a56b-388256781d5f";
constexpr auto kBenchmarkTimeout = std::chrono::seconds(120);
constexpr auto kBenchmarkInterval = std::chrono::microseconds(400);

void finishBenchmarkErrorLocked(const char* phase, const char* reason) {
    g_liveBenchmark.running = false;
    g_liveBenchmark.finished = true;
    g_liveBenchmark.passed = false;
    g_liveBenchmark.phase = phase;
    g_liveBenchmark.error = reason;
    g_liveBenchmarkEnd = std::chrono::steady_clock::now();
    g_liveHitType.store(-1, std::memory_order_release);
    disarmAdditionalLiveBindings();
    ++g_liveHitGeneration;
    addManagedCounter(g_liveHitStatus.dropped, g_liveHitQueue.size());
    g_liveHitQueue.clear();
    g_liveHitStatus.enabled = false;
    g_liveHitStatus.phase = phase;
}

void* managedBenchmarkProducer(void*) {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            if (!g_liveBenchmark.running) return nullptr;
            const auto now = std::chrono::steady_clock::now();
            if (g_liveHitStatus.failures != 0 ||
                (!g_liveHitStatus.enabled &&
                 g_liveHitStatus.phase != "limit-reached")) {
                finishBenchmarkErrorLocked("benchmark-failed", "Falha ou interrupcao do callback gerenciado");
                return nullptr;
            }
            if (now - g_liveBenchmarkStart > kBenchmarkTimeout) {
                finishBenchmarkErrorLocked("benchmark-timeout", "Tempo limite de 120 segundos");
                return nullptr;
            }
            if (g_liveHitStatus.queued == ManagedLiveEventQueue::kEventLimit) {
                g_liveBenchmark.phase = "draining";
                if (g_liveHitStatus.processed == ManagedLiveEventQueue::kEventLimit &&
                    g_liveHitStatus.phase == "limit-reached" &&
                    g_liveHitQueue.empty() && !g_liveHitInFlight &&
                    g_liveHitStatus.failures == 0) {
                    g_liveBenchmark.running = false;
                    g_liveBenchmark.finished = true;
                    g_liveBenchmark.passed = true;
                    g_liveBenchmark.phase = "completed";
                    g_liveBenchmarkEnd = now;
                    return nullptr;
                }
            } else if (g_liveHitQueue.size() < ManagedLiveEventQueue::kCapacity) {
                const int tick = g_liveHitStatus.queued + 1;
                if (g_liveHitQueue.push({tick, 18, (tick & 1) == 0})) {
                    ++g_liveHitStatus.queued;
                    g_liveHitStatus.maxQueueDepth = std::max(
                        g_liveHitStatus.maxQueueDepth, g_liveHitQueue.size());
                    if (g_liveHitStatus.queued >= ManagedLiveEventQueue::kEventLimit) {
                        g_liveHitStatus.phase = "draining";
                        g_liveBenchmark.phase = "draining";
                    }
                    g_pinnedWake.notify_one();
                }
            }
        }
        std::this_thread::sleep_for(kBenchmarkInterval);
    }
}

int64_t compareThreadCpuNs() {
    timespec point{};
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &point) == 0
        ? static_cast<int64_t>(point.tv_sec) * 1000000000LL + point.tv_nsec : 0;
}

int compareRssKb() {
    FILE* file = std::fopen("/proc/self/status", "rb");
    if (!file) return 0;
    int value = 0;
    char line[128]{};
    while (std::fgets(line, sizeof(line), file)) {
        if (std::sscanf(line, "VmRSS: %d", &value) == 1) break;
    }
    std::fclose(file);
    return value;
}

struct VmCompareMonoResult {
    bool ready = false;
    int result = 0;
    int64_t wallUs = 0;
    int64_t cpuUs = 0;
    int rssBeforeKb = 0;
    int rssAfterKb = 0;
    std::string error;
};

VmCompareMonoResult runVmCompareMono(
    const std::string& assemblyPath, int iterations, int seed, int kind) {
    VmCompareMonoResult output;
    if (kind < 0 || kind > 2 ||
        (kind == 0 && (iterations < 1000 || iterations > 5000000)) ||
        (kind != 0 && (iterations < 64 || iterations > 5000)) ||
        !validAssembly(assemblyPath)) {
        output.error = "Invalid Mono benchmark mode, duration, or probe";
        return output;
    }
    const auto slash = assemblyPath.find_last_of('/');
    if (slash == std::string::npos) {
        output.error = "Managed framework folder missing";
        return output;
    }
    const bool math = kind == 0;
    const char* assemblyName = math ? "BunnyVmBenchmark67.dll" : "BunnyVmGameplay69.dll";
    const std::string benchmarkAssembly = assemblyPath.substr(0, slash + 1) + assemblyName;
    if (!validAssembly(benchmarkAssembly)) {
        output.error = std::string(assemblyName) + " missing or invalid";
        return output;
    }
    std::lock_guard<std::mutex> monoLock(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke || !g_api.unbox) {
        output.error = "MonoVM not ready";
        return output;
    }
    if (!g_api.attach(g_domain)) {
        output.error = "MonoVM thread attach failed";
        return output;
    }
    void* assembly = g_api.openAssembly(g_domain, benchmarkAssembly.c_str());
    if (!assembly) {
        output.error = "BunnyVmBenchmark67.dll could not be opened";
        return output;
    }
    void* image = g_api.image(assembly);
    void* klass = image ? g_api.findClass(
        image, math ? "BunnyLoader.VmBenchmark67" : "BunnyLoader.VmGameplay69",
        "Kernel") : nullptr;
    if (!klass) {
        output.error = std::string(assemblyName) + " Kernel class missing";
        return output;
    }
    const char* methodName = kind == 1 ? "Projectile" : kind == 2 ? "Npc" : "Execute";
    void* method = g_api.findMethod(klass, methodName, 2);
    if (!method) {
        output.error = std::string(assemblyName) + " Kernel." + methodName + "(int,int) missing";
        return output;
    }
    auto call = [&](int n, int* result) {
        void* args[] = {&n, &seed};
        void* exception = nullptr;
        void* boxed = g_api.invoke(method, nullptr, args, &exception);
        void* unboxed = exception || !boxed ? nullptr : g_api.unbox(boxed);
        if (!unboxed) return false;
        std::memcpy(result, unboxed, sizeof(int));
        return true;
    };
    int warm = 0;
    if (!call(kind == 0 ? 4096 : 64, &warm)) {
        output.error = "Mono warm-up failed";
        return output;
    }
    output.rssBeforeKb = compareRssKb();
    const int64_t cpuStart = compareThreadCpuNs();
    const auto start = std::chrono::steady_clock::now();
    if (!call(iterations, &output.result)) {
        output.error = "Mono benchmark call failed";
        return output;
    }
    const auto end = std::chrono::steady_clock::now();
    const int64_t cpuEnd = compareThreadCpuNs();
    output.rssAfterKb = compareRssKb();
    output.wallUs = std::chrono::duration_cast<
        std::chrono::microseconds>(end - start).count();
    output.cpuUs = cpuEnd >= cpuStart ? (cpuEnd - cpuStart) / 1000 : 0;
    output.ready = true;
    return output;
}

void logManagedHurtFailure(const std::string& modId, int slot, const char* phase) {
    // Only called on the serialized Mono owner, on failures; no hot-path IO.
    static std::map<std::string, unsigned> reports;
    unsigned& count = reports[modId + "/" + phase];
    if (++count > 9) return;
    std::string text = "csharp callback: mod=" + modId + " fase=" + phase;
    if (count == 9) {
        text += " (novas repeticoes omitidas nesta sessao)";
    } else if (g_managedHurtErrorMethod) {
        using Utf8Fn = char* (*)(void*);
        using FreeFn = void (*)(void*);
        auto utf8 = resolve<Utf8Fn>("mono_string_to_utf8");
        auto release = resolve<FreeFn>("mono_free");
        void* args[] = {&slot};
        void* exception = nullptr;
        void* value = g_api.invoke(g_managedHurtErrorMethod, nullptr, args, &exception);
        if (!exception && value && utf8 && release) {
            char* message = utf8(value);
            if (message) {
                text += " ";
                text.append(message, strnlen(message, 2304));
                release(message);
            }
        }
    }
    if (auto logger = g_hurtDiagnosticLogger.load(std::memory_order_acquire))
        logger(text.c_str());
#ifdef __ANDROID__
    else __android_log_print(ANDROID_LOG_ERROR, "BunnyCSharp", "%s", text.c_str());
#endif
}

int64_t executeManagedHurtOnWorker(const ManagedHurtJob& job) {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.invoke || !g_api.unbox || !g_api.newString ||
        !g_api.openAssembly || !g_api.image || !g_api.findClass || !g_api.findMethod)
        return -1;
    if (!g_api.attach(g_domain)) return -1;
    std::string modId;
    std::string dll;
    int slot = -1;
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (job.modIndex < 0 ||
            static_cast<size_t>(job.modIndex) >= g_managedHurtBindings.size())
            return -1;
        const auto& binding = g_managedHurtBindings[static_cast<size_t>(job.modIndex)];
        if (!binding.active || binding.failed || job.generation != g_managedHurtGeneration ||
            (job.completion && (job.completion->cancelled ||
             std::chrono::steady_clock::now() >= job.completion->deadline))) return -1;
        modId = binding.modId;
        dll = binding.assemblyPath;
        slot = binding.managedSlot;
    }
    if (!g_managedHurtRegisterMethod) {
        const size_t slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return -1;
        const std::string sdk = g_checkpointPath.substr(0, slash + 1) + "BunnySourceSdk.dll";
        if (!validAssembly(sdk)) return -1;
        void* assembly = g_api.openAssembly(g_domain, sdk.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* cls = image ? g_api.findClass(
            image, "BunnyLoader.Source", "BunnyPlayerHurtDispatcher") : nullptr;
        if (!cls) return -1;
        g_managedHurtRegisterMethod = g_api.findMethod(cls, "Register", 2);
        g_managedHurtModifyMethod = g_api.findMethod(cls, "Modify", 21);
        g_managedHurtAfterMethod = g_api.findMethod(cls, "After", 21);
        g_managedHurtErrorMethod = g_api.findMethod(cls, "GetLastError", 1);
        g_managedHurtSnapshotMethod = g_api.findMethod(cls, "GetHookSnapshot", 0);
        if (!g_managedHurtRegisterMethod || !g_managedHurtModifyMethod ||
            !g_managedHurtAfterMethod) return -1;
    }
    if (slot < 0) {
        if (!validAssembly(dll)) return -1;
        void* args[] = {
            g_api.newString(g_domain, modId.c_str()),
            g_api.newString(g_domain, dll.c_str())
        };
        if (!args[0] || !args[1]) return -1;
        void* exception = nullptr;
        void* boxed = g_api.invoke(g_managedHurtRegisterMethod, nullptr, args, &exception);
        void* raw = exception || !boxed ? nullptr : g_api.unbox(boxed);
        if (!raw) return -1;
        std::memcpy(&slot, raw, sizeof(slot));
        if (slot < 0 || slot >= 16) {
            logManagedHurtFailure(modId, -1, "Register");
            return -1;
        }
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            if (static_cast<size_t>(job.modIndex) < g_managedHurtBindings.size() &&
                g_managedHurtBindings[static_cast<size_t>(job.modIndex)].modId == modId)
                g_managedHurtBindings[static_cast<size_t>(job.modIndex)].managedSlot = slot;
        }
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                            "BunnyPlayer Hurt MonoVM ready: %s (slot=%d)",
                            modId.c_str(), slot);
#endif
    }
    int d = job.damage;
    int origin = job.sourceDamage;
    int direction = job.direction;
    int flags = job.flags;
    int life = job.life;
    int lifeMax = job.maxLife;
    int mask = job.equipmentMask;
    int npcIndex = job.npcIndex;
    int projectileIndex = job.projectileIndex;
    int npcType = job.npcType;
    int npcDamage = job.npcDamage;
    int npcLife = job.npcLife;
    int projectileType = job.projectileType;
    int projectileDamage = job.projectileDamage;
    int projectileOwner = job.projectileOwner;
    float attackerX = job.attackerX;
    float attackerY = job.attackerY;
    float attackerVelocityX = job.attackerVelocityX;
    float attackerVelocityY = job.attackerVelocityY;
    float attackerAngle = job.attackerAngle;
    float attackerDistance = job.attackerDistance;
    void* argsModify[] = {&slot, &d, &direction, &flags, &life, &lifeMax, &mask,
                          &npcIndex, &projectileIndex, &npcType, &npcDamage, &npcLife,
                          &projectileType, &projectileDamage, &projectileOwner,
                          &attackerX, &attackerY, &attackerVelocityX, &attackerVelocityY,
                          &attackerAngle, &attackerDistance};
    void* argsAfter[] = {&slot, &d, &origin, &direction, &flags, &life, &mask,
                         &npcIndex, &projectileIndex, &npcType, &npcDamage, &npcLife,
                         &projectileType, &projectileDamage, &projectileOwner,
                         &attackerX, &attackerY, &attackerVelocityX, &attackerVelocityY,
                         &attackerAngle, &attackerDistance};
    void* exception = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (job.generation != g_managedHurtGeneration ||
            job.modIndex < 0 ||
            static_cast<size_t>(job.modIndex) >= g_managedHurtBindings.size() ||
            !g_managedHurtBindings[static_cast<size_t>(job.modIndex)].active ||
            (job.completion && (job.completion->cancelled ||
             std::chrono::steady_clock::now() >= job.completion->deadline))) return -1;
    }
    void* boxed = g_api.invoke(job.after ? g_managedHurtAfterMethod :
        g_managedHurtModifyMethod, nullptr,
        job.after ? argsAfter : argsModify, &exception);
    void* raw = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!raw) return -1;
    const auto snapshotNow = std::chrono::steady_clock::now();
    if (g_managedHurtSnapshotMethod && snapshotNow >= g_hurtSnapshotNext) {
        g_hurtSnapshotNext = snapshotNow + std::chrono::milliseconds(650);
        void* snapshotError = nullptr;
        void* snapshot = g_api.invoke(g_managedHurtSnapshotMethod, nullptr, nullptr, &snapshotError);
        if (!snapshotError && snapshot) {
            auto utf8 = resolve<char* (*)(void*)>("mono_string_to_utf8");
            auto release = resolve<void (*)(void*)>("mono_free");
            if (utf8 && release) {
                char* value = utf8(snapshot);
                if (value) {
                    std::lock_guard<std::mutex> lock(g_hurtSnapshotMutex);
                    g_hurtSnapshotText.assign(value, strnlen(value, 768));
                    release(value);
                }
            }
        }
    }
    if (job.after) {
        int done = 0;
        std::memcpy(&done, raw, sizeof(done));
        if (done < 0) logManagedHurtFailure(modId, slot, "After");
        return done >= 0 ? done : -1;
    }
    int64_t packed = -1;
    std::memcpy(&packed, raw, sizeof(packed));
    if (packed < 0) logManagedHurtFailure(modId, slot, "Modify");
    return packed;
}

void persistManagedEntityCombatSnapshot(uint64_t session, size_t pending) {
    std::string checkpoint;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        checkpoint = g_checkpointPath;
    }
    const size_t slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos) return;
    const std::string file = checkpoint.substr(0, slash + 1) + "entity-combat-status.txt";
    const std::string temp = file + ".tmp";
    const char* names[] = {"NPC.OnHit", "NPC.OnKill", "Projectile.OnHitNPC", "Projectile.OnKill"};
    std::string text = "state=observed\nsession=" + std::to_string(session) +
        "\nmods=" + std::to_string(g_entityModCount.load(std::memory_order_relaxed)) +
        "\nqueuedTotal=" + std::to_string(g_entityCombatSubmitted.load(std::memory_order_relaxed)) +
        "\ndroppedTotal=" + std::to_string(g_entityCombatDropped.load(std::memory_order_relaxed)) +
        "\nqueuePending=" + std::to_string(pending) +
        "\nnpcAiJobsProcessed=" + std::to_string(g_entityAiProcessed[0].load(std::memory_order_relaxed)) +
        "\nprojectileAiJobsProcessed=" + std::to_string(g_entityAiProcessed[1].load(std::memory_order_relaxed)) +
        "\nworkerWrites=" + std::to_string(g_entityCombatStatusWrites.load(std::memory_order_relaxed)) + "\n";
    for (size_t i = 0; i < 4; ++i) {
        text += std::string(names[i]) +
            " queued=" + std::to_string(g_entityCombatQueued[i].load(std::memory_order_relaxed)) +
            " executed=" + std::to_string(g_entityCombatExecuted[i].load(std::memory_order_relaxed)) +
            " rejected=" + std::to_string(g_entityCombatRejected[i].load(std::memory_order_relaxed)) +
            " error=" + std::to_string(g_entityCombatFaulted[i].load(std::memory_order_relaxed)) +
            " unbound=" + std::to_string(g_entityCombatUnbound[i].load(std::memory_order_relaxed)) +
            " dropped=" + std::to_string(g_entityCombatDroppedByKind[i].load(std::memory_order_relaxed)) +
            " stale=" + std::to_string(g_entityCombatLost[i].load(std::memory_order_relaxed)) + "\n";
    }
    FILE* output = std::fopen(temp.c_str(), "wb");
    if (!output) return;
    const bool good = std::fwrite(text.data(), 1, text.size(), output) == text.size();
    const bool closed = std::fclose(output) == 0;
    if (good && closed) {
        if (std::rename(temp.c_str(), file.c_str()) == 0)
            g_entityCombatStatusWrites.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::remove(temp.c_str());
    }
}

int64_t executeManagedEntityOnWorker(const ManagedEntityJob& job) {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke ||
        !g_api.unbox || !g_api.newString) return -1;
    if (!g_api.attach(g_domain)) return -1;

    ManagedEntityBinding binding;
    {
        std::lock_guard<std::mutex> guard(g_pinnedMutex);
        if (job.session != g_entitySession || job.binding < 0 ||
            static_cast<size_t>(job.binding) >= g_entityBindings.size() ||
            !g_entityBindings[static_cast<size_t>(job.binding)].armed) return -1;
        binding = g_entityBindings[static_cast<size_t>(job.binding)];
    }
    if (!g_entityRegister) {
        const size_t slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return -1;
        const std::string sdk = g_checkpointPath.substr(0, slash + 1) + "BunnySourceSdk.dll";
        if (!validAssembly(sdk)) return -1;
        void* assembly = g_api.openAssembly(g_domain, sdk.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, "BunnyLoader.Source",
                                             "BunnyEntitySourceDispatcher") : nullptr;
        if (!klass) return -1;
        g_entityRegister = g_api.findMethod(klass, "Register", 2);
        g_entityBind = g_api.findMethod(klass, "Bind", 4);
        g_entityNpcTick = g_api.findMethod(klass, "TickNpc", 13);
        g_entityWorldTick = g_api.findMethod(klass, "TickWorld", 7);
        g_entityStatusMethod = g_api.findMethod(klass, "Status", 1);
        g_entityProjectileTick = g_api.findMethod(klass, "TickProjectile", 14);
        g_entityNpcHit = g_api.findMethod(klass, "HitNpc", 6);
        g_entityNpcKill = g_api.findMethod(klass, "KillNpc", 4);
        g_entityProjectileHit = g_api.findMethod(klass, "HitProjectile", 7);
        g_entityProjectileKill = g_api.findMethod(klass, "KillProjectile", 5);
        if (!g_entityRegister || !g_entityBind ||
            !g_entityNpcTick || !g_entityWorldTick || !g_entityProjectileTick ||
            !g_entityNpcHit || !g_entityNpcKill ||
            !g_entityProjectileHit || !g_entityProjectileKill) return -1;
    }
    int managedSlot = binding.managedSlot;
    if (managedSlot < 0) {
        if (!validAssembly(binding.assemblyPath)) return -1;
        void* registerArgs[] = {
            g_api.newString(g_domain, binding.modId.c_str()),
            g_api.newString(g_domain, binding.assemblyPath.c_str())
        };
        if (!registerArgs[0] || !registerArgs[1]) return -1;
        void* exception = nullptr;
        void* boxed = g_api.invoke(g_entityRegister, nullptr, registerArgs, &exception);
        void* raw = exception || !boxed ? nullptr : g_api.unbox(boxed);
        if (!raw) return -1;
        std::memcpy(&managedSlot, raw, sizeof(managedSlot));
        if (managedSlot < 0 || managedSlot >= 16) return -1;

        auto bindTypes = [&](int kind, const std::vector<std::pair<int, std::string>>& values) {
            for (const auto& [id, name] : values) {
                void* stringValue = g_api.newString(g_domain, name.c_str());
                if (!stringValue) continue;
                void* args[] = {&managedSlot, &kind, const_cast<int*>(&id), stringValue};
                void* bindException = nullptr;
                g_api.invoke(g_entityBind, nullptr, args, &bindException);
                if (bindException) return false;
            }
            return true;
        };
        if (!bindTypes(0, binding.npcTypes) ||
            !bindTypes(1, binding.projectileTypes)) return -1;
        {
            std::lock_guard<std::mutex> guard(g_pinnedMutex);
            if (job.session == g_entitySession && job.binding >= 0 &&
                static_cast<size_t>(job.binding) < g_entityBindings.size() &&
                g_entityBindings[static_cast<size_t>(job.binding)].armed &&
                g_entityBindings[static_cast<size_t>(job.binding)].modId == binding.modId)
                g_entityBindings[static_cast<size_t>(job.binding)].managedSlot = managedSlot;
        }
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                            "CSharp Entity MonoVM ready mod=%s npc=%zu projectile=%zu",
                            binding.modId.c_str(), binding.npcTypes.size(),
                            binding.projectileTypes.size());
#endif
    }
    if (job.kind == 6) {
        int mode = job.owner, frames = job.life, player = job.target;
        int64_t worldSession = static_cast<int64_t>(job.identity);
        float playerX = job.x, playerY = job.y;
        void* args[] = {&managedSlot, &mode, &frames, &worldSession,
                        &player, &playerX, &playerY};
        void* exception = nullptr;
        void* boxed = g_api.invoke(g_entityWorldTick, nullptr, args, &exception);
        void* raw = exception || !boxed ? nullptr : g_api.unbox(boxed);
        if (!raw) return -1;
        int32_t count = -1;
        std::memcpy(&count, raw, sizeof(count));
        if (count > 0 && g_entityStatusMethod) {
            bool needsUseStatus = false;
            {
                std::lock_guard<std::mutex> guard(g_pinnedMutex);
                if (job.session == g_entitySession && job.binding >= 0 &&
                    static_cast<size_t>(job.binding) < g_entityBindings.size())
                    needsUseStatus = g_entityBindings[static_cast<size_t>(job.binding)].useReports < 12;
            }
            if (needsUseStatus) {
                void* statusArgs[] = {&managedSlot};
                void* statusError = nullptr;
                void* status = g_api.invoke(g_entityStatusMethod, nullptr, statusArgs, &statusError);
                if (status && !statusError) {
                    auto utf8 = resolve<char* (*)(void*)>("mono_string_to_utf8");
                    auto release = resolve<void (*)(void*)>("mono_free");
                    if (utf8 && release) {
                        char* message = utf8(status);
                        if (message) {
                            std::string details(message, strnlen(message, 384));
                            release(message);
                            const auto use = details.find("lastUse=");
                            if (use != std::string::npos &&
                                details.compare(use + 8, 4, "None") != 0) {
                                const size_t end = details.find(" npc=", use);
                                const std::string outcome = details.substr(use,
                                    end == std::string::npos ? end : end - use);
                                [[maybe_unused]] bool report = false;
                                {
                                    std::lock_guard<std::mutex> guard(g_pinnedMutex);
                                    if (job.session == g_entitySession && job.binding >= 0 &&
                                        static_cast<size_t>(job.binding) < g_entityBindings.size()) {
                                        auto& owner = g_entityBindings[static_cast<size_t>(job.binding)];
                                        if (owner.useReports < 12 && owner.lastUseOutcome != outcome) {
                                            owner.firstWorldUseReported = true;
                                            owner.lastUseOutcome = outcome;
                                            ++owner.useReports;
                                            report = true;
                                        }
                                    }
                                }
#ifdef __ANDROID__
                                if (report)
                                    reportManagedItemUse(
                                        outcome.find(" Error=") != std::string::npos
                                            ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
                                        "[CSharp][ItemUse] Mod=" + binding.modId +
                                            " Stage=OnUseResult " + outcome);
#endif
                            }
                        }
                    }
                }
            }
        }
        bool reportError = false;
        if (count < 0) {
            std::lock_guard<std::mutex> guard(g_pinnedMutex);
            if (job.session == g_entitySession && job.binding >= 0 &&
                static_cast<size_t>(job.binding) < g_entityBindings.size()) {
                auto& owner = g_entityBindings[static_cast<size_t>(job.binding)];
                if (!owner.firstWorldErrorReported) {
                    owner.firstWorldErrorReported = true;
                    reportError = true;
                }
            }
        }
        if (reportError) {
            std::string detail = "WorldTick returned " + std::to_string(count);
            if (g_entityStatusMethod) {
                void* statusArgs[] = {&managedSlot};
                void* statusError = nullptr;
                void* status = g_api.invoke(g_entityStatusMethod, nullptr,
                    statusArgs, &statusError);
                if (status && !statusError) {
                    auto utf8 = resolve<char* (*)(void*)>("mono_string_to_utf8");
                    auto release = resolve<void (*)(void*)>("mono_free");
                    if (utf8 && release) {
                        char* message = utf8(status);
                        if (message) {
                            detail.assign(message, strnlen(message, 320));
                            release(message);
                        }
                    }
                }
            }
#ifdef __ANDROID__
            reportManagedItemUse(ANDROID_LOG_WARN,
                "[CSharp][ItemUse] Mod=" + binding.modId +
                " Stage=WorldTick Result=Error Detail=" + detail);
#endif
        }
        return count;
    }
    int type = job.type, slot = job.slot, generation = job.generation;
    int life = job.life, maxLife = job.lifeMax, owner = job.owner;
    float x = job.x, y = job.y, vx = job.vx, vy = job.vy;
    float px = job.playerX, py = job.playerY;
    if (job.kind >= 2 && job.kind <= 5) {
        int damage = job.damage, target = job.target;
        void* npcHit[] = {&managedSlot, &type, &slot, &generation, &damage, nullptr};
        uint8_t killed = 0;
        npcHit[5] = &killed;
        void* npcKill[] = {&managedSlot, &type, &slot, &generation};
        void* projectileHit[] = {&managedSlot, &type, &slot, &generation,
                                 &owner, &target, &damage};
        void* projectileKill[] = {&managedSlot, &type, &slot, &generation, &owner};
        void* chosen = job.kind == 2 ? g_entityNpcHit :
                       job.kind == 3 ? g_entityNpcKill :
                       job.kind == 4 ? g_entityProjectileHit : g_entityProjectileKill;
        void** chosenArgs = job.kind == 2 ? npcHit :
                            job.kind == 3 ? npcKill :
                            job.kind == 4 ? projectileHit : projectileKill;
        void* exception = nullptr;
        void* boxed = g_api.invoke(chosen, nullptr, chosenArgs, &exception);
        if (exception || !boxed) return -1;
        void* raw = g_api.unbox(boxed);
        return raw && *static_cast<uint8_t*>(raw) ? 1 : 0;
    }
    int64_t nativeIdentity = static_cast<int64_t>(job.identity);
    void* npcArgs[] = {&managedSlot, &type, &slot, &generation,
                       &life, &maxLife, &x, &y, &vx, &vy, &px, &py,
                       &nativeIdentity};
    int targetNpc = job.target;
    float targetX = job.targetX, targetY = job.targetY;
    void* projectileArgs[] = {&managedSlot, &type, &slot, &generation, &owner,
                              &x, &y, &vx, &vy, &px, &py,
                              &targetNpc, &targetX, &targetY};
    void* exception = nullptr;
    void* boxed = g_api.invoke(job.kind == 0 ? g_entityNpcTick : g_entityProjectileTick,
                               nullptr, job.kind == 0 ? npcArgs : projectileArgs,
                               &exception);
    void* raw = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!raw) return -1;
    int64_t encoded = -1;
    std::memcpy(&encoded, raw, sizeof(encoded));
    return encoded;
}

bool prepareUniversalHookSdk() {
    if (g_universalRegister && g_universalSnapshot && g_universalDispatch && g_universalDispatchInt32 && g_universalDispatchInt32Pair && g_universalDispatchExtended && g_universalClear && g_universalUnloadMod &&
        g_universalRemove && g_universalPauseWorld) return true;
    if (!g_domain || !g_status.methodExecuted || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke || !g_api.unbox) return false;
    const auto slash = g_checkpointPath.find_last_of('/');
    if (slash == std::string::npos) return false;
    const std::string sdk = g_checkpointPath.substr(0, slash + 1) + "BunnySourceSdk.dll";
    void* assembly = g_api.openAssembly(g_domain, sdk.c_str());
    void* image = assembly ? g_api.image(assembly) : nullptr;
    void* dispatcher = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyUniversalHookDispatcher") : nullptr;
    void* hooks = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyHooks") : nullptr;
    if (!dispatcher || !hooks) return false;
    g_universalRegister = g_api.findMethod(dispatcher, "Register", 2);
    g_universalClear = g_api.findMethod(dispatcher, "UnloadAll", 0);
    g_universalPauseWorld = g_api.findMethod(dispatcher, "PauseWorld", 0);
    g_universalUnloadMod = g_api.findMethod(dispatcher, "Unload", 1);
    g_universalError = g_api.findMethod(dispatcher, "get_LastError", 0);
    g_universalSnapshot = g_api.findMethod(dispatcher, "GetStatusSnapshot", 1);
    g_universalDispatch = g_api.findMethod(hooks, "Invoke", 4);
    g_universalDispatchInt32 = g_api.findMethod(hooks, "InvokeInt32", 6);
    g_universalDispatchInt32Pair = g_api.findMethod(hooks, "InvokeInt32Pair", 7);
    g_universalDispatchExtended = g_api.findMethod(hooks, "InvokeExtended", 9);
    g_universalRemove = g_api.findMethod(hooks, "RemoveSlot", 1);
    return g_universalRegister && g_universalSnapshot && g_universalDispatch && g_universalDispatchInt32 &&
           g_universalDispatchInt32Pair && g_universalDispatchExtended &&
           g_universalClear && g_universalUnloadMod && g_universalRemove && g_universalPauseWorld;
}

void writeUniversalRegistrationStatus(const std::string& id, const char* state, const std::string& detail) {
    const size_t slash = g_checkpointPath.find_last_of('/');
    if (slash == std::string::npos) return;
    const std::string path = g_checkpointPath.substr(0, slash + 1) + "universal-hooks-status.txt";
    const std::string temp = path + ".tmp";
    FILE* file = std::fopen(temp.c_str(), "wb");
    if (!file) return;
    const std::string value = "timestampUnix=" + std::to_string(static_cast<long long>(std::time(nullptr))) +
        "\npid=" + std::to_string(static_cast<long long>(getpid())) +
        "\nmodId=" + id + "\nstate=" + state +
        "\ndetail=" + detail.substr(0, 1600) + "\n";
    const bool saved = std::fwrite(value.data(), 1, value.size(), file) == value.size() &&
        std::fflush(file) == 0;
    std::fclose(file);
    if (saved) std::rename(temp.c_str(), path.c_str());
    else std::remove(temp.c_str());
}

std::string managedHookSnapshot(const std::string& id) {
    if (!g_universalSnapshot || !g_api.invoke || !g_api.newString) return "snapshot indisponivel";
    void* idString = g_api.newString(g_domain, id.c_str());
    if (!idString) return "id mono indisponivel";
    void* args[] = {idString};
    void* exception = nullptr;
    void* value = g_api.invoke(g_universalSnapshot, nullptr, args, &exception);
    if (exception || !value) return "snapshot falhou";
    using ToUtf8 = char* (*)(void*);
    using Release = void (*)(void*);
    auto convert = resolve<ToUtf8>("mono_string_to_utf8");
    auto freeString = resolve<Release>("mono_free");
    if (!convert || !freeString) return "conversor nao encontrado";
    char* text = convert(value);
    if (!text) return "snapshot vazio";
    const std::string result(text, strnlen(text, 1600));
    freeString(text);
    return result;
}

struct UniversalMonoContext {
    explicit UniversalMonoContext(bool allowInstall = false) {
        universalCanInstall = allowInstall;
        universalLeaveMono = +[] { g_mutex.unlock(); };
        universalResumeMono = +[] { g_mutex.lock(); };
    }
    ~UniversalMonoContext() { universalCanInstall = false; universalLeaveMono = universalResumeMono = nullptr; }
};

bool pollUniversalHooksOnWorker() {
    UniversalHookMailbox::Work work{};
    if (universalHookMailbox().take(work)) {
        bool success = false;
        bool retryable = false;
        int32_t returnedValue = 0;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (prepareUniversalHookSdk()) {
                int slot = work.slot;
                int64_t ticket = static_cast<int64_t>(work.ticket);
                uintptr_t self = work.self;
                int result = 0;
                void* exception = nullptr;
                UniversalMonoContext context;
                if (work.extendedMode > 0) {
                    int32_t first = work.argument;
                    int32_t second = work.argument2;
                    int32_t third = work.argument3;
                    int mode = work.extendedMode;
                    void* args[] = {&slot, &ticket, &self, &first, &second, &third, &mode, &result, &returnedValue};
                    g_api.invoke(g_universalDispatchExtended, nullptr, args, &exception);
                } else if (work.int32PairMode) {
                    int32_t first = work.argument;
                    int32_t second = work.argument2;
                    void* args[] = {&slot, &ticket, &self, &first, &second, &result, &returnedValue};
                    g_api.invoke(g_universalDispatchInt32Pair, nullptr, args, &exception);
                } else if (work.int32Mode) {
                    int32_t argument = work.argument;
                    void* args[] = {&slot, &ticket, &self, &argument, &result, &returnedValue};
                    g_api.invoke(g_universalDispatchInt32, nullptr, args, &exception);
                } else {
                    void* args[] = {&slot, &ticket, &self, &result};
                    g_api.invoke(g_universalDispatch, nullptr, args, &exception);
                }
                success = !exception && result == 1;
                retryable = !exception && result == 2;
            }
        }
        const bool accepted = universalHookMailbox().complete(work.ticket, success, returnedValue, retryable);
        if (!accepted) noteUniversalHookLateCompletion();
        if (universalHookCallbackIsTerminal(accepted, success, retryable)) {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_universalRemove) {
                void* args[] = {&work.slot};
                void* exception = nullptr;
                g_api.invoke(g_universalRemove, nullptr, args, &exception);
            }
        }
        return true;
    }
    UniversalMod next;
    std::string targetUnload;
    bool unload = false, found = false, targeted = false;
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        unload = g_universalUnload;
        g_universalUnload = false;
        if (unload) {
            g_universalPendingUnloads.clear();
        } else if (!g_universalPendingUnloads.empty()) {
            targetUnload = std::move(g_universalPendingUnloads.front());
            g_universalPendingUnloads.pop_front();
            targeted = true;
        }
        if (!unload && !targeted && g_universalEnabled) {
            for (auto& mod : g_universalMods) {
                if (mod.registered || g_revokedDeferredMods.contains(mod.id))
                    continue;
                mod.registered = true;
                next = mod;
                found = true;
                break;
            }
        }
    }
    if (!unload && !found && !targeted) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!prepareUniversalHookSdk()) {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (targeted) g_universalPendingUnloads.push_front(targetUnload);
        if (found) {
            for (auto& mod : g_universalMods)
                if (mod.id == next.id) mod.registered = false;
        }
        g_universalPolling.store(false, std::memory_order_release);
        return false;
    }
    void* exception = nullptr;
    UniversalMonoContext context(!unload && !targeted);
    if (unload) {
        std::vector<std::string> ids;
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            for (const auto& mod : g_universalMods) ids.push_back(mod.id);
        }
        void* boxed=g_api.invoke(g_universalPauseWorld,nullptr,nullptr,&exception);
        void* value=!exception && boxed?g_api.unbox(boxed):nullptr;
        int retained=0;if(value)std::memcpy(&retained,value,sizeof(retained));
        for (const std::string& id : ids)
            writeUniversalRegistrationStatus(id, "paused", managedHookSnapshot(id));
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (!g_universalEnabled) g_universalPolling.store(retained>0, std::memory_order_release);
    }
    else if (targeted && g_api.newString) {
        void* id = g_api.newString(g_domain, targetUnload.c_str());
        void* args[] = {id};
        void* boxed = id ? g_api.invoke(g_universalUnloadMod, nullptr, args, &exception) : nullptr;
        void* value = !exception && boxed ? g_api.unbox(boxed) : nullptr;
        if (!value) {
            writeUniversalRegistrationStatus(targetUnload, "unload-failed",
                "SDK Unload(id) unavailable or threw; restart required");
        } else if (*static_cast<uint8_t*>(value) == 0) {
            writeUniversalRegistrationStatus(targetUnload, "already-unloaded",
                "no active managed hook scope for this mod");
        } else {
            writeUniversalRegistrationStatus(targetUnload, "unloaded",
                "native hook scope released; Mono assembly remains loaded");
        }
    }
    else if (found && g_api.newString) {
        bool revoked = false;
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            revoked = g_revokedDeferredMods.contains(next.id);
        }
        if (revoked) return true;
        void* args[] = {g_api.newString(g_domain, next.id.c_str()), g_api.newString(g_domain, next.path.c_str())};
        void* boxed = args[0] && args[1] ? g_api.invoke(g_universalRegister, nullptr, args, &exception) : nullptr;
        int count = -1;
        void* raw = !exception && boxed ? g_api.unbox(boxed) : nullptr;
        if (raw) std::memcpy(&count, raw, sizeof(count));
        if (count >= 0) {
            writeUniversalRegistrationStatus(next.id, "registered",
                "modules=" + std::to_string(count) + " " + managedHookSnapshot(next.id));
        }
        else {
            writeUniversalRegistrationStatus(next.id, "failed",
                "mono=" + std::to_string(exception != nullptr) +
                " " + managedHookSnapshot(next.id));
            std::string line = "csharp universal hooks: registro recusado mod=" + next.id;
            using Utf8 = char* (*)(void*);
            using Free = void (*)(void*);
            auto utf8 = resolve<Utf8>("mono_string_to_utf8");
            auto release = resolve<Free>("mono_free");
            if (g_universalError && utf8 && release) {
                void* errorException = nullptr;
                void* text = g_api.invoke(g_universalError, nullptr, nullptr, &errorException);
                char* detail = !errorException && text ? utf8(text) : nullptr;
                if (detail) { line += " "; line.append(detail, strnlen(detail, 1024)); release(detail); }
            }
            if (auto logger = g_hurtDiagnosticLogger.load(std::memory_order_acquire)) logger(line.c_str());
        }
    }
    return true;
}

template<class Execute>
bool processManagedRecipeRpc(std::unique_lock<std::mutex>& lock,Execute execute) {
    if(g_recipeRpcQueue.empty())return false;
    auto job=std::move(g_recipeRpcQueue.front());g_recipeRpcQueue.pop_front();
    if(job.completion->cancelled||std::chrono::steady_clock::now()>=job.deadline){
        if((job.shooting||job.tile||job.rarity)&&job.kind==2&&!job.completion->cancelled){job.completion->done=true;job.completion->output="ERROR|Stage=Warmup Reason=WarmupDeadlineExceeded";}
        g_recipeRpcWake.notify_all();return true;
    }
    lock.unlock();auto output=execute(job);lock.lock();
    if(!job.completion->cancelled&&std::chrono::steady_clock::now()<job.deadline) {
        job.completion->output=std::move(output);job.completion->done=true;
    } else if((job.shooting||job.tile||job.rarity)&&job.kind==2&&!job.completion->cancelled) {
        job.completion->output="ERROR|Stage=Warmup Reason=WarmupDeadlineExceeded";job.completion->done=true;
    }
    g_recipeRpcWake.notify_all();return true;
}

std::string executeRecipeOnWorker(const RecipeRpcJob& job) {
    if (!job.shooting && !job.tile && !job.rarity &&
        !isNativeRecipeAssemblyCurrent(job.mod, job.assembly))
        return "PENDING|Stage=Invoke Reason=AssemblyUnloadedOrRebound";
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if(!g_domain||!g_status.methodExecuted||!g_api.attach||!g_api.openAssembly||!g_api.image||
       !g_api.findClass||!g_api.findMethod||!g_api.newString||!g_api.invoke)return "PENDING|Stage=Invoke Reason=MonoRuntimeOrRequiredExportsNotReady";
    if(!g_api.attach(g_domain))return "ERROR|Stage=AttachThread Reason=MonoAttachFailed";
    void*& dispatcher=job.rarity?g_rarityDispatchMethod:(job.tile?g_tileDispatchMethod:(job.shooting?g_shootDispatchMethod:g_recipeDispatchMethod));
    const char* endpoint=job.rarity?"BunnyRarityRuntime":(job.tile?"BunnyTileRuntime":(job.shooting?"BunnyShootRuntime":"BunnyRecipeRuntime"));
    if(!dispatcher) {
        const auto slash=g_checkpointPath.find_last_of('/');
        if(slash==std::string::npos)return "ERROR|Stage=ResolveSdk Reason=InvalidCheckpointPath";
        const auto sdk=g_checkpointPath.substr(0,slash+1)+"BunnySourceSdk.dll";
        if(!validAssembly(sdk))return "ERROR|Stage=ResolveSdk Path="+sdk+" Reason=SdkAssemblyMissingOrInvalid";
        auto* assembly=g_api.openAssembly(g_domain,sdk.c_str());
        auto* image=assembly?g_api.image(assembly):nullptr;
        auto* cls=image?g_api.findClass(image,"BunnyLoader.Source",endpoint):nullptr;
        dispatcher=cls?g_api.findMethod(cls,"Dispatch",6):nullptr;
        if(!dispatcher)return std::string("ERROR|Stage=BindDispatcher Class=")+endpoint+" Method=Dispatch Expected=SixParameters Reason=MethodMissing";
    }
    void* args[]={g_api.newString(g_domain,job.mod.c_str()),g_api.newString(g_domain,job.assembly.c_str()),
        g_api.newString(g_domain,job.owner.c_str()),const_cast<int*>(&job.index),const_cast<int*>(&job.kind),
        g_api.newString(g_domain,job.snapshot.c_str())};
    if(!args[0]||!args[1]||!args[2]||!args[5])return "ERROR|Stage=MarshalArguments Reason=MonoStringAllocationFailed";
    void* exception=nullptr;auto* result=g_api.invoke(dispatcher,nullptr,args,&exception);
    if(exception||!result)return "ERROR|Reason=MonoDispatchException";
    using Utf8=char*(*)(void*);using Release=void(*)(void*);
    static auto utf8=resolve<Utf8>("mono_string_to_utf8");static auto release=resolve<Release>("mono_free");
    if(!utf8||!release)return "ERROR|Stage=DecodeResponse Expected=mono_string_to_utf8Andmono_free Reason=RequiredExportMissing";
    char* value=utf8(result);if(!value)return "ERROR|Stage=DecodeResponse Reason=MonoUtf8AllocationFailed";
    const size_t maximum=job.shooting?32768u:16384u;
    const auto length=strnlen(value,maximum+1);std::string output;
    if(length<=maximum)output.assign(value,length);
    else output="ERROR|Stage=DecodeResponse ExpectedBytes=UpTo"+std::to_string(maximum)+" ActualAtLeast="+std::to_string(maximum+1)+" Reason=ResponseTooLarge";
    release(value);return output;
}

std::string executeManagedLocalizationOnWorker(const LocalizationJob& job) {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_status.methodExecuted || !g_api.attach ||
        !g_api.openAssembly || !g_api.image || !g_api.findClass ||
        !g_api.findMethod || !g_api.newString || !g_api.invoke) return "PENDING|MonoNotReady";
    if (!g_api.attach(g_domain)) return "ERROR|MonoAttachFailed";
    if (!g_localizationDispatchMethod || !g_localizationUnloadMethod ||
        !g_localizationDrainMethod || !g_localizationAckMethod) {
        const auto slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return "ERROR|InvalidSdkPath";
        const std::string sdk = g_checkpointPath.substr(0, slash + 1) + "BunnySourceSdk.dll";
        if (!validAssembly(sdk)) return "ERROR|MissingSdk";
        void* assembly = g_api.openAssembly(g_domain, sdk.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyLocalizationRuntime") : nullptr;
        if (!klass) return "ERROR|MissingLocalizationRuntime";
        g_localizationDispatchMethod = g_api.findMethod(klass, "Dispatch", 3);
        g_localizationDrainMethod = g_api.findMethod(klass, "Drain", 3);
        g_localizationAckMethod = g_api.findMethod(klass, "Acknowledge", 4);
        g_localizationUnloadMethod = g_api.findMethod(klass, "Unload", 1);
        if (!g_localizationDispatchMethod || !g_localizationDrainMethod ||
            !g_localizationUnloadMethod || !g_localizationAckMethod)
            return "ERROR|MissingLocalizationMethods";
    }
    void* id = g_api.newString(g_domain, job.modId.c_str());
    if (!id) return "ERROR|MonoIdAllocation";
    void* exception = nullptr;
    if (job.unload) {
        void* args[] = {id};
        g_api.invoke(g_localizationUnloadMethod, nullptr, args, &exception);
        return exception ? "ERROR|MonoUnloadException" : "OK|Unload";
    }
    void* assembly = g_api.newString(g_domain, job.assemblyPath.c_str());
    void* culture = g_api.newString(g_domain, job.culture.c_str());
    if (!assembly || !culture) return "ERROR|MonoStringAllocation";
    void* args[] = {id, assembly, culture};
    void* value = g_api.invoke(job.drain ? g_localizationDrainMethod :
                               g_localizationDispatchMethod, nullptr, args, &exception);
    if (exception || !value) return "ERROR|MonoDispatchException";
    using Utf8 = char* (*)(void*);
    using Release = void (*)(void*);
    static auto utf8 = resolve<Utf8>("mono_string_to_utf8");
    static auto release = resolve<Release>("mono_free");
    if (!utf8 || !release) return "ERROR|MonoStringExportsMissing";
    char* text = utf8(value);
    if (!text) return "ERROR|MonoStringDecode";
    const size_t length = strnlen(text, 262145);
    std::string result = length <= 262144 ? std::string(text, length) : "ERROR|ResponseTooLarge";
    release(text);
    return result;
}

bool acknowledgeManagedLocalizationOnWorker(const LocalizationJob& job,
                                          const std::string& wire) {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_api.attach || !g_api.newString || !g_api.invoke ||
        !g_api.unbox || !g_localizationAckMethod || !g_api.attach(g_domain)) return false;
    void* args[] = {
        g_api.newString(g_domain, job.modId.c_str()),
        g_api.newString(g_domain, job.assemblyPath.c_str()),
        g_api.newString(g_domain, job.culture.c_str()),
        g_api.newString(g_domain, wire.c_str())
    };
    for (void* arg : args) if (!arg) return false;
    void* exception = nullptr;
    void* result = g_api.invoke(g_localizationAckMethod, nullptr, args, &exception);
    void* value = exception || !result ? nullptr : g_api.unbox(result);
    return value && *static_cast<uint8_t*>(value) != 0;
}

bool executeManagedSceneOnWorker(const SceneLightRequest& request,
                                ManagedScreenLightResult* result) {
    if (!result) return false;
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_status.methodExecuted || !g_api.attach ||
        !g_api.openAssembly || !g_api.image || !g_api.findClass ||
        !g_api.findMethod || !g_api.newString || !g_api.invoke) return false;
    if (!g_api.attach(g_domain)) return false;
    SceneLightBinding binding;
    {
        std::lock_guard<std::mutex> guard(g_pinnedMutex);
        if (request.generation != g_sceneLightGeneration || request.binding < 0 ||
            static_cast<size_t>(request.binding) >= g_sceneLightBindings.size()) return false;
        binding = g_sceneLightBindings[static_cast<size_t>(request.binding)];
    }
    if (!binding.enabled || !validAssembly(binding.assemblyPath)) return false;
    const std::string cacheKey = binding.assemblyPath + ":" + binding.nameSpace +
                                 ":" + binding.className + ":" + binding.methodName;
    void*& method = g_sceneLightMethods[cacheKey];
    if (!method) {
        void* assembly = g_api.openAssembly(g_domain, binding.assemblyPath.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, binding.nameSpace.c_str(),
                                              binding.className.c_str()) : nullptr;
        method = klass ? g_api.findMethod(klass, binding.methodName.c_str(), 1) : nullptr;
        if (!method) return false;
    }
    char input[440]{};
    const auto& raw = request.raw;
    const int n = std::snprintf(input, sizeof input,
        "%d|%.12g|%.12g|%.9g|%.9g|%d|%d|%.9g|%d|%d|%d|%d|%d|%d|%d|%d|%d|%.9g|%.9g|%d",
        raw.dayTime ? 1 : 0, raw.gameTime, raw.worldSurface,
        raw.cameraX, raw.cameraY, raw.screenWidth, raw.screenHeight,
        raw.playerCenterY, raw.moonPhase, raw.invertedGravity ? 1 : 0,
        raw.skyRed, raw.skyGreen, raw.skyBlue,
        raw.snow ? 1 : 0, raw.crimson ? 1 : 0, raw.corrupt ? 1 : 0,
        raw.snowBackground, raw.snowAlpha, raw.desertAlpha, raw.desertBackground);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof input) return false;
    void* monoInput = g_api.newString(g_domain, input);
    if (!monoInput) return false;
    void* args[1] = {monoInput};
    void* exception = nullptr;
    void* boxed = g_api.invoke(method, nullptr, args, &exception);
    if (exception || !boxed) return false;
    using Utf8Fn = char* (*)(void*);
    using ReleaseFn = void (*)(void*);
    static auto utf8 = resolve<Utf8Fn>("mono_string_to_utf8");
    static auto release = resolve<ReleaseFn>("mono_free");
    if (!utf8 || !release) return false;
    char* data = utf8(boxed);
    if (!data) return false;
    float sunX = 0, sunY = 0, clock = 0, strength = 0, threshold = 0;
    int enabled = 0;
    char extra = 0;
    const bool parsed = strnlen(data, 256) < 256 &&
        std::sscanf(data, "%f;%f;%f;%f;%f;%d%c",
                    &sunX, &sunY, &clock, &strength, &threshold, &enabled,
                    &extra) == 6;
    release(data);
    if (!parsed || !std::isfinite(sunX) || !std::isfinite(sunY) ||
        !std::isfinite(clock) || !std::isfinite(strength) ||
        !std::isfinite(threshold) || sunX < -2.f || sunX > 3.f ||
        sunY < -2.f || sunY > 3.f || clock < 0.f || clock > 2.f ||
        strength < 0.f || strength > 3.f || threshold < 0.f ||
        threshold > 1.f || (enabled != 0 && enabled != 1)) return false;
    *result = {sunX, sunY, clock, strength, threshold,
               request.baseSunX, request.baseSunY, request.baseClock,
               enabled == 1};
    return true;
}

bool executeManagedDustOnWorker(int index,
                               const ManagedDustMailbox::Request* request,
                               ManagedDustState* updated, uint32_t* flags) {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke ||
        !g_api.unbox || !g_api.newString || !g_api.attach(g_domain)) return false;
    ManagedDustBinding binding;
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (index < 0 || static_cast<size_t>(index) >= g_dustBindings.size() ||
            !g_dustBindings[static_cast<size_t>(index)].enabled) return false;
        binding = g_dustBindings[static_cast<size_t>(index)];
    }
    if (!g_dustRegisterMethod) {
        const size_t slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return false;
        const std::string sdk = g_checkpointPath.substr(0, slash + 1) +
                                "BunnySourceSdk.dll";
        if (!validAssembly(sdk)) return false;
        void* sdkAssembly = g_api.openAssembly(g_domain, sdk.c_str());
        void* sdkImage = sdkAssembly ? g_api.image(sdkAssembly) : nullptr;
        void* cls = sdkImage ? g_api.findClass(sdkImage, "BunnyLoader.Source",
                                            "BunnyDustSourceDispatcher") : nullptr;
        if (!cls) return false;
        g_dustRegisterMethod = g_api.findMethod(cls, "Register", 2);
        g_dustBindMethod = g_api.findMethod(cls, "Bind", 3);
        g_dustInvokeMethod = g_api.findMethod(cls, "Invoke", 3);
        g_dustResetMethod = g_api.findMethod(cls, "ResetWorld", 0);
        if (!g_dustRegisterMethod || !g_dustBindMethod ||
            !g_dustInvokeMethod || !g_dustResetMethod) {
            g_dustRegisterMethod = nullptr;
            return false;
        }
    }
    int managedSlot = binding.managedSlot;
    if (managedSlot < 0) {
        if (!validAssembly(binding.assemblyPath)) return false;
        void* argStrings[2] = {
            g_api.newString(g_domain, binding.modId.c_str()),
            g_api.newString(g_domain, binding.assemblyPath.c_str())
        };
        if (!argStrings[0] || !argStrings[1]) return false;
        void* error = nullptr;
        void* boxed = g_api.invoke(g_dustRegisterMethod, nullptr,
                                   argStrings, &error);
        void* raw = error || !boxed ? nullptr : g_api.unbox(boxed);
        if (!raw) return false;
        std::memcpy(&managedSlot, raw, sizeof(managedSlot));
        if (managedSlot < 0 || managedSlot >= 16) return false;
        for (const auto& [type, name] : binding.types) {
            void* nameArg = g_api.newString(g_domain, name.c_str());
            if (!nameArg) return false;
            int nativeType = type;
            void* args[3] = {&managedSlot, &nativeType, nameArg};
            error = nullptr;
            boxed = g_api.invoke(g_dustBindMethod, nullptr, args, &error);
            raw = error || !boxed ? nullptr : g_api.unbox(boxed);
            if (!raw || !*static_cast<uint8_t*>(raw)) return false;
        }
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            if (static_cast<size_t>(index) >= g_dustBindings.size() ||
                g_dustBindings[static_cast<size_t>(index)].modId != binding.modId)
                return false;
            g_dustBindings[static_cast<size_t>(index)].managedSlot = managedSlot;
        }
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                            "[CSharp][DustLoader] Stage=MonoBind Mod=%s Types=%zu Result=Ready",
                            binding.modId.c_str(), binding.types.size());
#endif
    }
    if (index >= 0 && static_cast<size_t>(index) < g_dustReady.size())
        g_dustReady[static_cast<size_t>(index)].store(true, std::memory_order_release);
    if (!request) return true;
    if (!updated || !flags || request->binding != index ||
        !managedDustStateValid(request->state)) return false;
    ManagedDustState localState = request->state;
    int phase = request->phase;
    void* statePointer = &localState;
    void* args[] = {&managedSlot, &phase, &statePointer};
    void* error = nullptr;
    void* boxed = g_api.invoke(g_dustInvokeMethod, nullptr, args, &error);
    void* raw = error || !boxed ? nullptr : g_api.unbox(boxed);
    if (!raw) return false;
    int result = 0;
    std::memcpy(&result, raw, sizeof(result));
    if (result < 0 || result > 63 ||
        !managedDustStateValid(localState) ||
        !managedDustSameInstance(request->state, localState)) return false;
    *updated = localState;
    *flags = static_cast<uint32_t>(result);
    return true;
}

void executeManagedDustResetOnWorker() {
    std::lock_guard<std::mutex> monoGuard(g_mutex);
    if (!g_domain || !g_dustResetMethod || !g_api.attach ||
        !g_api.invoke || !g_api.attach(g_domain)) return;
    void* error = nullptr;
    g_api.invoke(g_dustResetMethod, nullptr, nullptr, &error);
#ifdef __ANDROID__
    if (error)
        __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                            "[CSharp][DustLoader] Stage=ResetWorld Result=ManagedException");
#endif
}

bool prepareManagedCommandWorker() {
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke || !g_api.newString ||
        !g_api.unbox)
        return false;
    if (!g_api.attach(g_domain)) return false;
    if (g_commandRegisterMethod && g_commandExecuteMethod &&
        g_commandClearMethod) return true;
    const size_t slash = g_checkpointPath.find_last_of('/');
    if (slash == std::string::npos) return false;
    const std::string sdk = g_checkpointPath.substr(0, slash + 1) + "BunnySourceSdk.dll";
    if (!validAssembly(sdk)) return false;
    void* assembly = g_api.openAssembly(g_domain, sdk.c_str());
    void* image = assembly ? g_api.image(assembly) : nullptr;
    void* klass = image ? g_api.findClass(image, "BunnyLoader.Source",
                                           "BunnyCommandWorker") : nullptr;
    void* registerMethod = klass ? g_api.findMethod(klass, "RegisterAssembly", 2) : nullptr;
    void* executeMethod = klass ? g_api.findMethod(klass, "Execute", 4) : nullptr;
    void* clearMethod = klass ? g_api.findMethod(klass, "Clear", 0) : nullptr;
    if (!registerMethod || !executeMethod || !clearMethod) return false;
    g_commandRegisterMethod = registerMethod;
    g_commandExecuteMethod = executeMethod;
    g_commandClearMethod = clearMethod;
    return true;
}

bool clearManagedCommandOnWorker() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!prepareManagedCommandWorker()) return false;
    void* exception = nullptr;
    g_api.invoke(g_commandClearMethod, nullptr, nullptr, &exception);
    return !exception;
}

bool registerManagedCommandOnWorker(const ManagedCommandBinding& binding) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!validAssembly(binding.assembly) || !prepareManagedCommandWorker()) return false;
    void* args[] = {
        g_api.newString(g_domain, binding.modId.c_str()),
        g_api.newString(g_domain, binding.assembly.c_str())
    };
    if (!args[0] || !args[1]) return false;
    void* exception = nullptr;
    void* boxed = g_api.invoke(g_commandRegisterMethod, nullptr, args, &exception);
    void* raw = !exception && boxed ? g_api.unbox(boxed) : nullptr;
    if (!raw) return false;
    int count = 0;
    std::memcpy(&count, raw, sizeof(count));
    return count > 0 && count <= 128;
}

std::string executeManagedCommandOnWorker(const NativeCommandRequest& request) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!prepareManagedCommandWorker()) return {};
    int player = request.playerIndex;
    int mode = request.networkMode;
    int kind = static_cast<int>(request.caller);
    void* args[] = {
        g_api.newString(g_domain, request.input.c_str()),
        &player, &mode, &kind
    };
    if (!args[0]) return {};
    void* exception = nullptr;
    void* result = g_api.invoke(g_commandExecuteMethod, nullptr, args, &exception);
    if (exception || !result) return {};
    using Utf8Fn = char* (*)(void*);
    using FreeFn = void (*)(void*);
    static const auto utf8 = resolve<Utf8Fn>("mono_string_to_utf8");
    static const auto release = resolve<FreeFn>("mono_free");
    if (!utf8 || !release) return {};
    char* raw = utf8(result);
    if (!raw) return {};
    const size_t bytes = strnlen(raw, 20 * 1024 + 1);
    std::string wire;
    if (bytes > 0 && bytes <= 20 * 1024)
        wire.assign(raw, bytes);
    release(raw);
    return wire;
}

void* pinnedMonoWorker(void*) {
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        g_pinnedTid = static_cast<long>(syscall(__NR_gettid));
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                            "pinned MonoVM owner started tid=%ld", g_pinnedTid);
#endif
    }
    for (;;) {
        std::string assembly;
        std::string stage;
        {
            std::unique_lock<std::mutex> lock(g_pinnedMutex);
            while (!g_pinnedPending) {
                if(processManagedRecipeRpc(lock,executeRecipeOnWorker))continue;
                const bool localizationReady =
                    g_recipeRuntimeReady.load(std::memory_order_acquire);
                const bool livePolling = localizationReady &&
                    !g_localizationLiveBindings.empty() &&
                    std::chrono::steady_clock::now() >= g_nextLocalizationPoll;
                if (localizationReady &&
                    (!g_localizationUnloads.empty() || !g_localizationQueue.empty() ||
                     livePolling)) {
                    LocalizationJob job;
                    if (!g_localizationUnloads.empty()) {
                        job.modId = std::move(g_localizationUnloads.front());
                        g_localizationUnloads.pop_front();
                        job.unload = true;
                    } else if (!g_localizationQueue.empty()) {
                        job = std::move(g_localizationQueue.front());
                        g_localizationQueue.pop_front();
                    } else {
                        auto it = g_localizationLiveBindings.upper_bound(g_lastLocalizationPollMod);
                        if (it == g_localizationLiveBindings.end())
                            it = g_localizationLiveBindings.begin();
                        job = it->second;
                        job.drain = true;
                        g_lastLocalizationPollMod = it->first;
                        g_nextLocalizationPoll = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(250);
                    }
                    if (!job.unload && !g_localizationEpochFence.current(job.modId, job.generation))
                        continue;
                    lock.unlock();
                    std::string result = executeManagedLocalizationOnWorker(job);
                    if (!job.unload && result.rfind("OK|", 0) == 0) {
                        auto sink = g_localizationResultSink.load(std::memory_order_acquire);
                        if (!sink || !sink(job.modId, job.assemblyPath, job.culture,
                                           job.generation, result))
                            result = "ERROR|DynamicWireRejectedOrStale";
                        else if (!acknowledgeManagedLocalizationOnWorker(job, result))
                            result = "ERROR|DynamicWireAckFailed";
                    }
                    if (result.rfind("OK|", 0) != 0 && result != "UNCHANGED") {
#ifdef __ANDROID__
                        __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                            "[CSharp][Localization] Mod=%s Stage=ManagedNotify Result=%s",
                            job.modId.c_str(), result.c_str());
#else
                        std::fprintf(stderr,
                            "[CSharp][Localization] Mod=%s Stage=ManagedNotify Result=%s\n",
                            job.modId.c_str(), result.c_str());
#endif
                    }
                    lock.lock();
                    continue;
                }

                if (g_universalPolling.load(std::memory_order_acquire)) {
                    lock.unlock();
                    const bool handled = pollUniversalHooksOnWorker();
                    lock.lock();
                    if (handled) continue;
                }
                if (!g_managedHurtQueue.empty()) {
                    ManagedHurtJob job = std::move(g_managedHurtQueue.front());
                    g_managedHurtQueue.pop_front();
                    if (job.generation != g_managedHurtGeneration ||
                        (job.completion && (job.completion->cancelled ||
                         std::chrono::steady_clock::now() >= job.completion->deadline))) {
                        ++g_hurtScheduler.cancelledBeforeRun;
                        if (job.completion) {
                            job.completion->expired = std::chrono::steady_clock::now() >= job.completion->deadline;
                            job.completion->done = true;
                            g_managedHurtResultWake.notify_all();
                        }
                        continue;
                    }
                    lock.unlock();
                    const int64_t outcome = executeManagedHurtOnWorker(job);
                    if (job.after && job.damage > 0) {
                        static std::atomic<int> npcLogs{0};
                        static std::atomic<int> projectileLogs{0};
                        static std::atomic<int> otherLogs{0};
                        const bool fromNpc = job.npcIndex >= 0;
                        const bool fromProjectile = !fromNpc && job.projectileIndex >= 0;
                        const int logged = (fromNpc ? npcLogs :
                                            fromProjectile ? projectileLogs : otherLogs)
                                               .fetch_add(1, std::memory_order_relaxed);
                        if (logged < 8) {
                            char line[256];
                            std::snprintf(line, sizeof(line),
                                "csharp BunnyPlayer hit diag: MonoVM %s estado=%s instancias=%lld npc=%d projectile=%d dano=%d vida=%d",
                                fromNpc ? "OnHitByNPC" :
                                fromProjectile ? "OnHitByProjectile" : "OnHurt",
                                outcome >= 0 ? "concluido" : "falhou",
                                static_cast<long long>(outcome),
                                job.npcIndex, job.projectileIndex,
                                job.damage, job.life);
                            if (auto logger = g_hurtDiagnosticLogger.load(std::memory_order_acquire))
                                logger(line);
#ifdef __ANDROID__
                            else
                                __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp", "%s", line);
#endif
                        }
                    }
                    lock.lock();
                    if (job.completion) {
                        const bool bindingActive = job.modIndex >= 0 &&
                            static_cast<size_t>(job.modIndex) < g_managedHurtBindings.size() &&
                            g_managedHurtBindings[static_cast<size_t>(job.modIndex)].active;
                        if (!bindingActive) job.completion->cancelled = true;
                        job.completion->expired = std::chrono::steady_clock::now() >= job.completion->deadline;
                        if (job.completion->cancelled || job.generation != g_managedHurtGeneration ||
                            job.completion->expired)
                            ++g_hurtScheduler.lateResults;
                        else {
                            job.completion->value = outcome;
                            ++g_hurtScheduler.completed;
                        }
                        job.completion->done = true;
                        g_managedHurtResultWake.notify_all();
                    }
                    continue;
                }
                if (g_vmComparePending) {
                    g_vmComparePending = false;
                    const int iterations = g_vmCompare.iterations;
                    const int seed = g_vmCompare.seed;
                    const int kind = g_vmCompare.kind;
                    const std::string assembly = g_pinnedAssembly;
                    lock.unlock();
                    const VmCompareMonoResult done =
                        runVmCompareMono(assembly, iterations, seed, kind);
                    lock.lock();
                    g_vmCompare.monoResult = done.result;
                    g_vmCompare.monoWallUs = done.wallUs;
                    g_vmCompare.monoThreadCpuUs = done.cpuUs;
                    g_vmCompare.rssBeforeMonoKb = done.rssBeforeKb;
                    g_vmCompare.rssAfterMonoKb = done.rssAfterKb;
                    g_vmCompare.running = false;
                    g_vmCompare.finished = true;
                    g_vmCompare.passed = done.ready &&
                        done.result == g_vmCompare.jsResult;
                    g_vmCompare.phase = !done.ready ? "failed" :
                        g_vmCompare.passed ? "matched" : "mismatch";
                    g_vmCompare.error = done.error;
                    continue;
                }
                if (g_commandSessionReset &&
                    g_recipeRuntimeReady.load(std::memory_order_acquire)) {
                    g_commandSessionReset = false;
                    lock.unlock();
                    const bool cleared = clearManagedCommandOnWorker();
                    lock.lock();
                    for (auto& binding : g_commandBindings) {
                        binding.registered = false;
                        binding.failures = cleared ? 0 : 3;
                    }
                    continue;
                }
                if (!g_commandBindings.empty() &&
                    g_recipeRuntimeReady.load(std::memory_order_acquire)) {
                    bool attemptedRegistration = false;
                    for (const auto& pending : g_commandBindings) {
                        if (pending.registered || pending.failures >= 3) continue;
                        ManagedCommandBinding binding = pending;
                        lock.unlock();
                        const bool registered = registerManagedCommandOnWorker(binding);
                        lock.lock();
                        for (auto& current : g_commandBindings) {
                            if (current.modId != binding.modId ||
                                current.assembly != binding.assembly) continue;
                            if (registered) {
                                current.registered = true;
                                nativeCommandMailbox().setModReady(binding.modId, true);
                            } else ++current.failures;
                            break;
                        }
                        attemptedRegistration = true;
                        break;
                    }
                    if (attemptedRegistration) continue;
                    auto commands = nativeCommandMailbox().takeRequests(8);
                    if (!commands.empty()) {
                        lock.unlock();
                        for (const auto& command : commands) {
                            if (command.epoch != nativeCommandMailbox().epoch())
                                continue;
                            const std::string wire = executeManagedCommandOnWorker(command);
                            std::vector<NativeCommandReply> messages;
                            bool handled = false;
                            NativeCommandAction action;
                            std::string parseError;
                            const bool valid = !wire.empty() &&
                                decodeNativeCommandResult(wire, command.playerIndex,
                                                          &handled, &messages, &parseError,
                                                          &action) && handled;
                            if (valid && wire.find("\nACTION\t") != std::string::npos) {
                                if (nativeCommandMailbox().deliverAction(command, action))
                                    continue;
                            }
                            if (!valid ||
                                wire.find("\nACTION\t") != std::string::npos) {
                                messages.clear();
                                NativeCommandReply fallback;
                                fallback.playerIndex = command.playerIndex;
                                fallback.text = "Comando C# indisponivel";
                                fallback.red = 255;
                                fallback.green = 25;
                                fallback.blue = 25;
                                messages.push_back(std::move(fallback));
                            }
                            nativeCommandMailbox().deliver(command.epoch,
                                                           command.sequence, messages);
                        }
                        lock.lock();
                        continue;
                    }
                }
                ManagedLiveQueuedEvent event;
                bool hasEvent = g_liveHitQueue.pop(&event);
                if (!hasEvent && g_liveIngress.pop(&event)) {
                    hasEvent = true;
                    event.tick = addManagedCounter(g_liveHitStatus.queued);
                    g_liveHitStatus.maxQueueDepth = std::max(
                        g_liveHitStatus.maxQueueDepth, g_liveIngress.maxDepth());
                    if (g_liveHitStatus.eventLimit > 0 &&
                        g_liveHitStatus.queued >= g_liveHitStatus.eventLimit)
                        g_liveHitStatus.phase = "draining";
                }
                if (hasEvent) {
                    g_liveHitInFlight = true;
                    g_liveHitActiveBindingSlot = event.bindingSlot;
                    const bool automatic = g_liveHitStatus.automatic;
                    const bool additional = event.bindingSlot >= 0 &&
                        event.bindingSlot < g_additionalLiveCount.load(
                            std::memory_order_acquire);
                    AdditionalLiveBinding* binding = additional ?
                        &g_additionalLiveBindings[static_cast<size_t>(event.bindingSlot)] : nullptr;
                    if (binding && binding->armedType.load(
                            std::memory_order_acquire) != binding->type) {
                        addManagedCounter(binding->dropped);
                        addManagedCounter(g_liveHitStatus.dropped);
                        g_liveHitInFlight = false;
                        g_liveHitActiveBindingSlot = -2;
                        continue;
                    }
                    const std::string modId = binding ? binding->modId : g_liveHitStatus.modId;
                    const std::string dll = binding ? binding->assemblyPath : g_liveHitAssembly;
                    const std::string callbackClass = binding ?
                        binding->callbackClass : "BunnyLiveEvents";
                    if (g_revokedDeferredMods.contains(modId)) {
                        addManagedCounter(g_liveHitStatus.dropped);
                        if (binding) addManagedCounter(binding->dropped);
                        g_liveHitInFlight = false;
                        g_liveHitActiveBindingSlot = -2;
                        continue;
                    }
                    const uint64_t generation = g_liveHitGeneration;
                    const bool samplePerf = g_perfState.load(std::memory_order_relaxed) == 2;
                    const size_t perfSlot = binding ?
                        static_cast<size_t>(event.bindingSlot + 1) : 0;
                    const int64_t perfBeginNs = samplePerf ? perfNowNs() : 0;
                    const int64_t cpuBeginNs = samplePerf ? perfThreadCpuNs() : 0;
                    if (samplePerf && event.enqueuedAtNs && perfBeginNs >= 0 &&
                        perfBeginNs >= static_cast<int64_t>(event.enqueuedAtNs)) {
                        const int64_t delay = (perfBeginNs -
                            static_cast<int64_t>(event.enqueuedAtNs)) / 1000;
                        g_perfQueueTotalUs.fetch_add(delay, std::memory_order_relaxed);
                        g_perfQueueSamples.fetch_add(1, std::memory_order_relaxed);
                        auto& projectilePerf = g_perfProjectileCounters[perfSlot];
                        projectilePerf.queueTotalUs.fetch_add(delay, std::memory_order_relaxed);
                        projectilePerf.queueSamples.fetch_add(1, std::memory_order_relaxed);
                        updatePerfMax(projectilePerf.queueMaxUs,
                            static_cast<int>(std::min<int64_t>(delay, 2147483647)));
                        perfRecordUs(g_perfQueueBins, delay);
                        updatePerfMax(g_perfQueueMaxUs,
                            static_cast<int>(std::min<int64_t>(delay, 2147483647)));
                    }
                    lock.unlock();
                    const LiveCallbackResult callback = automatic
                        ? invokeLiveSourceEvent(modId, dll, event.isKill, event.tick,
                                                 event.damage, event.npcIndex, event.npc, event.player, callbackClass)
                        : [&]() {
                            const LiveSourceStatus status =
                                invokeLiveSourceTick(modId, dll, event.tick, event.damage);
                            return LiveCallbackResult{
                                status.ready && status.phase == "ready",
                                status.lastResult, status.phase, status.error};
                        }();
                    if (samplePerf) {
                        const int64_t cpuEndNs = perfThreadCpuNs();
                        const int64_t elapsed = (perfNowNs() - perfBeginNs) / 1000;
                        const int64_t cpuUs = cpuBeginNs > 0 && cpuEndNs >= cpuBeginNs
                            ? (cpuEndNs - cpuBeginNs) / 1000 : -1;
                        g_perfCallbackTotalUs.fetch_add(elapsed, std::memory_order_relaxed);
                        perfRecordUs(g_perfCallbackBins, elapsed);
                        updatePerfMax(g_perfCallbackMaxUs,
                            static_cast<int>(std::min<int64_t>(elapsed, 2147483647)));
                        if (cpuUs >= 0) {
                            g_perfCallbackCpuTotalUs.fetch_add(cpuUs, std::memory_order_relaxed);
                            perfRecordUs(g_perfCallbackCpuBins, cpuUs);
                            updatePerfMax(g_perfCallbackCpuMaxUs,
                                static_cast<int>(std::min<int64_t>(cpuUs, 2147483647)));
                        }
                        g_perfCallbacks.fetch_add(1, std::memory_order_relaxed);
                        g_perfProcessed.fetch_add(1, std::memory_order_relaxed);
                        auto& projectilePerf = g_perfProjectileCounters[perfSlot];
                        projectilePerf.callbacks.fetch_add(1, std::memory_order_relaxed);
                        (event.isKill ? projectilePerf.kills : projectilePerf.hits)
                            .fetch_add(1, std::memory_order_relaxed);
                        projectilePerf.callbackTotalUs.fetch_add(
                            elapsed, std::memory_order_relaxed);
                        perfRecordUs(projectilePerf.callbackBins, elapsed);
                        updatePerfMax(projectilePerf.callbackMaxUs,
                            static_cast<int>(std::min<int64_t>(elapsed, 2147483647)));
                        if (cpuUs >= 0) {
                            projectilePerf.cpuTotalUs.fetch_add(
                                cpuUs, std::memory_order_relaxed);
                            updatePerfMax(projectilePerf.cpuMaxUs,
                                static_cast<int>(std::min<int64_t>(cpuUs, 2147483647)));
                        }
                        if (!callback.ready || callback.phase != "ready") {
                            g_perfFailed.fetch_add(1, std::memory_order_relaxed);
                            projectilePerf.failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    lock.lock();
                    g_liveHitInFlight = false;
                    g_liveHitActiveBindingSlot = -2;
                    const bool bindingArmed = !binding ||
                        binding->armedType.load(std::memory_order_acquire) == binding->type;
                    if (g_liveHitGeneration == generation &&
                        managedLiveCallbackCurrent(g_liveHitStatus.enabled, bindingArmed,
                            g_revokedDeferredMods.contains(modId))) {
                        if (automatic && !event.isKill && callback.usedV4 &&
                            callback.ready && callback.phase == "ready" &&
                            callback.value >= 1000000 && callback.value < 3000000) {
                            int buff = 0;
                            int ticks = 0;
                            int direction = 0;
                            int tenths = 0;
                            const bool recognized =
                                decodeManagedNpcBuffCode(callback.value, &buff, &ticks) ||
                                decodeManagedNpcImpulseCode(callback.value, &direction, &tenths);
                            if (g_buffHookReady.load(std::memory_order_acquire) &&
                                managedNpcSnapshotIsValid(event.npc) && recognized) {
                                const auto now = std::chrono::steady_clock::now();
                                const int64_t nowMs =
                                    std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now.time_since_epoch()).count();
                                const ManagedNpcBuffCommand request{
                                    event.npcIndex, event.npc.npcType,
                                    event.npc.posX, event.npc.posY,
                                    buff, ticks,
                                    g_buffEffectGeneration.load(std::memory_order_acquire),
                                    nowMs, direction, tenths,
                                    event.bindingSlot,
                                    g_buffOwnerGenerations[
                                        static_cast<size_t>(event.bindingSlot + 1)].load(
                                            std::memory_order_acquire)
                                };
                                if (g_buffEffectQueue.push(request))
                                    g_buffRequested.fetch_add(1, std::memory_order_relaxed);
                                else
                                    g_buffQueueDrops.fetch_add(1, std::memory_order_relaxed);
                            } else {
                                g_buffRejected.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        if (automatic && !event.isKill && callback.invoked) {
                            const bool succeeded = callback.ready && callback.phase == "ready";
                            if (callback.usedV4) {
                                g_liveHitStatus.hitContext.recordV4(
                                    event.npcIndex, event.npc, event.player, succeeded);
                                if (binding)
                                    binding->hitContext.recordV4(
                                        event.npcIndex, event.npc, event.player, succeeded);
                            } else if (callback.usedV3) {
                                g_liveHitStatus.hitContext.recordV3(
                                    event.npcIndex, event.npc, succeeded);
                                if (binding)
                                    binding->hitContext.recordV3(
                                        event.npcIndex, event.npc, succeeded);
                            } else {
                                g_liveHitStatus.hitContext.record(
                                    callback.usedV2, event.npcIndex, succeeded);
                                if (binding)
                                    binding->hitContext.record(
                                        callback.usedV2, event.npcIndex, succeeded);
                            }
                        }
                        if (callback.ready && callback.phase == "ready") {
                            addManagedCounter(g_liveHitStatus.processed);
                            if (event.isKill) addManagedCounter(g_liveHitStatus.killProcessed);
                            else addManagedCounter(g_liveHitStatus.hitProcessed);
                            if (binding) {
                                addManagedCounter(binding->processed);
                                if (event.isKill) addManagedCounter(binding->kills);
                                else addManagedCounter(binding->hits);
                                binding->lastResult = callback.value;
                            }
                            g_liveHitStatus.lastDamage = event.damage;
                            g_liveHitStatus.lastResult = callback.value;
                            g_liveHitStatus.lastEvent = event.isKill ? "OnKill" : "OnHitNPC";
                            if (g_liveHitStatus.failures == 0) {
                                g_liveHitStatus.phase = "ready";
                                g_liveHitStatus.error.clear();
                            }
                        } else {
                            addManagedCounter(g_liveHitStatus.failures);
                            g_liveHitStatus.phase = callback.phase;
                            g_liveHitStatus.error = callback.error;
                            if (binding) {
                                addManagedCounter(binding->failures);
                                binding->error = callback.error;
                                if (binding->failures >= 3)
                                    binding->armedType.store(-1, std::memory_order_release);
                            }
                            if (!binding) ++g_primaryLiveFailures;
                            if (!binding && g_primaryLiveFailures >= 3) {
                                addManagedCounter(g_liveHitStatus.dropped,
                                    g_liveIngress.pauseBinding(-1, g_liveHitType));
                                bool hasActiveBinding = false;
                                for (int i = 0; i < g_additionalLiveCount.load(std::memory_order_acquire); ++i) {
                                    const auto& other = g_additionalLiveBindings[static_cast<size_t>(i)];
                                    hasActiveBinding |= other.armedType.load(std::memory_order_acquire) == other.type;
                                }
                                if (!hasActiveBinding) {
                                    g_liveHitStatus.enabled = false;
                                    addManagedCounter(g_liveHitStatus.dropped, g_liveHitQueue.size());
                                    g_liveHitQueue.clear();
                                    addManagedCounter(g_liveHitStatus.dropped, g_liveIngress.clear());
                                    g_liveHitStatus.phase = "disabled";
                                } else g_liveHitStatus.phase = "primary-disabled";
                            }
                        }
                        if (g_liveHitStatus.enabled &&
                            g_liveHitStatus.eventLimit > 0 &&
                            g_liveHitStatus.queued >= g_liveHitStatus.eventLimit &&
                            g_liveHitQueue.empty() && g_liveIngress.depth() == 0) {
                            g_liveHitType.store(-1, std::memory_order_release);
                            disarmAdditionalLiveBindings();
                            g_liveHitStatus.enabled = false;
                            g_liveHitStatus.phase = "limit-reached";
                        }
                    }
                    continue;
                }
                // Recipe UI has a 3ms deadline; do not queue it behind an AI backlog.
                if(processManagedRecipeRpc(lock,executeRecipeOnWorker))continue;
                if (g_dustWorldResetPending) {
                    g_dustWorldResetPending = false;
                    lock.unlock();
                    executeManagedDustResetOnWorker();
                    lock.lock();
                    continue;
                }
                if (!g_dustBootstrap.empty()) {
                    const int index = g_dustBootstrap.front();
                    g_dustBootstrap.pop_front();
                    lock.unlock();
                    [[maybe_unused]] const bool bound = executeManagedDustOnWorker(index, nullptr, nullptr, nullptr);
#ifdef __ANDROID__
                    if (!bound)
                        reportManagedItemUse(ANDROID_LOG_WARN,
                            "[CSharp][DustLoader] Stage=MonoBind Result=Rejected Reason=ManagedRegisterOrBindFailed");
#endif
                    lock.lock();
                    continue;
                }
                ManagedDustMailbox::Request dustJobs[8]{};
                const size_t dustCount = g_dustMailbox.take(dustJobs, 8);
                if (dustCount > 0) {
                    lock.unlock();
                    for (size_t i = 0; i < dustCount; ++i) {
                        ManagedDustState updated{};
                        uint32_t callbackFlags = 0;
                        const auto& job = dustJobs[i];
                        const uint64_t now = g_dustGameFrame.load(std::memory_order_acquire);
                        const bool timely = job.phase == 5 ||
                            (now >= job.frame &&
                             now - job.frame <= ManagedDustMailbox::kMaxResultAgeFrames);
                        const bool invoked = timely && g_dustMailbox.current(job) &&
                            executeManagedDustOnWorker(job.binding, &job,
                                                       &updated, &callbackFlags);
                        if (invoked && job.phase != 5) {
                            if (!g_dustMailbox.complete(job, updated, callbackFlags))
                                g_dustMailbox.discard(job);
                        } else {
                            g_dustMailbox.discard(job);
                        }
                    }
                    lock.lock();
                    continue;
                }

                if (!g_entityQueue.empty()) {
                    ManagedEntityJob job = std::move(g_entityQueue.front());
                    g_entityQueue.pop_front();
                    lock.unlock();
                    const int64_t result = executeManagedEntityOnWorker(job);
                    lock.lock();
                    if (job.kind == 6 && job.session == g_entitySession &&
                        job.binding >= 0 &&
                        static_cast<size_t>(job.binding) < g_entityBindings.size()) {
                        auto& source = g_entityBindings[static_cast<size_t>(job.binding)];
                        if (!source.firstWorldTickReported) {
                            source.firstWorldTickReported = true;
#ifdef __ANDROID__
                            reportManagedItemUse(ANDROID_LOG_INFO,
                                "[CSharp][ItemUse] Mod=" + source.modId +
                                " Stage=FirstWorldTick Result=" + std::to_string(result));
#endif
                        }
                        if (result == -2) source.invasionSupported = false;
                    }
                    const bool stillArmed = job.binding >= 0 &&
                        static_cast<size_t>(job.binding) < g_entityBindings.size() &&
                        g_entityBindings[static_cast<size_t>(job.binding)].armed;
                    if (job.session == g_entitySession && job.slot >= 0 && stillArmed) {
                        if (job.kind == 0 && static_cast<size_t>(job.slot) < g_entityNpcResults.size()) {
                            g_entityNpcPending[static_cast<size_t>(job.slot)] = false;
                            g_entityNpcResults[static_cast<size_t>(job.slot)] = {
                                job.type, job.generation, job.owner,
                                job.identity, result, true
                            };
                        } else if (job.kind == 1 &&
                                   static_cast<size_t>(job.slot) < g_entityProjectileResults.size()) {
                            g_entityProjectilePending[static_cast<size_t>(job.slot)] = false;
                            g_entityProjectileResults[static_cast<size_t>(job.slot)] = {
                                job.type, job.generation, job.owner,
                                job.identity, result, true
                            };
                        }
                        g_entityJobsCompleted.fetch_add(1, std::memory_order_relaxed);
                        if (job.kind == 0 || job.kind == 1)
                            g_entityAiProcessed[static_cast<size_t>(job.kind)]
                                .fetch_add(1, std::memory_order_relaxed);
                        if (job.kind >= 2 && job.kind <= 5) {
                            const int eventIndex=job.kind-2;
                            if (result == 0)
                                g_entityCombatRejected[static_cast<size_t>(eventIndex)]
                                    .fetch_add(1, std::memory_order_relaxed);
                            if (result < 0)
                                g_entityCombatFaulted[static_cast<size_t>(eventIndex)]
                                    .fetch_add(1, std::memory_order_relaxed);
                        }
                        if (job.kind >= 2 && job.kind <= 5 && result == 1) {
                            const int eventIndex=job.kind-2;
                            const int observed=g_entityCombatExecuted[static_cast<size_t>(eventIndex)]
                                .fetch_add(1, std::memory_order_relaxed);
                            if (observed == 0) {
#ifdef __ANDROID__
                                const char* names[]={
                                    "NPC.OnHit", "NPC.OnKill", "Projectile.OnHitNPC", "Projectile.OnKill"
                                };
                                __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                                    "csharp source combat: %s executado no MonoVM type=%d slot=%d generation=%d",
                                    names[eventIndex], job.type, job.slot, job.generation);
#endif
                            }
                        }
                    } else {
                        if (job.session == g_entitySession && !stillArmed && job.slot >= 0) {
                            if (job.kind == 0 &&
                                static_cast<size_t>(job.slot) < g_entityNpcPending.size())
                                g_entityNpcPending[static_cast<size_t>(job.slot)] = false;
                            if (job.kind == 1 &&
                                static_cast<size_t>(job.slot) < g_entityProjectilePending.size())
                                g_entityProjectilePending[static_cast<size_t>(job.slot)] = false;
                        }
                        if (job.kind >= 2 && job.kind <= 5)
                            g_entityCombatLost[static_cast<size_t>(job.kind - 2)]
                                .fetch_add(1, std::memory_order_relaxed);
                    }
                    if (job.kind >= 0 && job.kind <= 5) {
                        static std::chrono::steady_clock::time_point nextAiSnapshot{};
                        static std::chrono::steady_clock::time_point nextCombatSnapshot{};
                        const auto now = std::chrono::steady_clock::now();
                        const bool combat = job.kind >= 2;
                        if ((combat && now >= nextCombatSnapshot) ||
                            (!combat && now >= nextAiSnapshot)) {
                            if (combat) nextCombatSnapshot = now + std::chrono::seconds(1);
                            else nextAiSnapshot = now + std::chrono::seconds(10);
                            const uint64_t session = g_entitySession;
                            const size_t pending = g_entityQueue.size();
                            lock.unlock();
                            persistManagedEntityCombatSnapshot(session, pending);
                            lock.lock();
                        }
                    }
                    continue;
                }
                if (!g_sceneLightQueue.empty()) {
                    SceneLightRequest request = std::move(g_sceneLightQueue.front());
                    g_sceneLightQueue.pop_front();
                    lock.unlock();
                    ManagedScreenLightResult computed;
                    const bool ok = executeManagedSceneOnWorker(request, &computed);
                    lock.lock();
                    if (request.generation == g_sceneLightGeneration &&
                        request.binding >= 0 &&
                        static_cast<size_t>(request.binding) < g_sceneLightBindings.size()) {
                        auto& binding = g_sceneLightBindings[static_cast<size_t>(request.binding)];
                        if (binding.enabled && ok) {
                            g_sceneLightResults[binding.modId] = {
                                computed, std::chrono::steady_clock::now(), request.generation
                            };
                            binding.failures = 0;
                        } else if (binding.enabled && ++binding.failures >= 3) {
                            binding.enabled = false;
#ifdef __ANDROID__
                            __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                                "ScreenFx managed callback disabled mod=%s; native fallback active",
                                binding.modId.c_str());
#endif
                        }
                    }
                    continue;
                }
                if (!g_auditEnabled) {
                    if (g_universalPolling.load(std::memory_order_acquire) ||
                        g_liveHitStatus.enabled || g_liveIngress.depth() != 0 ||
                        !g_managedHurtQueue.empty() || !g_entityQueue.empty() ||
                        !g_dustBootstrap.empty() || g_dustWorldResetPending)
                        g_pinnedWake.wait_for(lock, std::chrono::milliseconds(2));
                    else if (g_dustModCount.load(std::memory_order_acquire) > 0)
                        g_pinnedWake.wait_for(lock, std::chrono::milliseconds(8));
                    else if (g_recipeRuntimeReady.load(std::memory_order_acquire) &&
                             !g_localizationLiveBindings.empty())
                        g_pinnedWake.wait_until(lock, g_nextLocalizationPoll);
                    else
                        g_pinnedWake.wait(lock);
                    continue;
                }
                const int seconds = kAuditSamples[g_auditIndex];
                const auto until = g_auditStart + std::chrono::seconds(seconds);
                auto deadline = (g_liveHitStatus.enabled || g_universalPolling.load(std::memory_order_acquire))
                    ? std::min(until, std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(2)) : until;
                if (g_recipeRuntimeReady.load(std::memory_order_acquire) &&
                    !g_localizationLiveBindings.empty())
                    deadline = std::min(deadline, g_nextLocalizationPoll);
                if (g_dustModCount.load(std::memory_order_acquire)>0)
                    deadline=std::min(deadline,std::chrono::steady_clock::now()+std::chrono::milliseconds(8));
                if (g_pinnedWake.wait_until(lock, deadline, [] {
                    return g_pinnedPending || g_vmComparePending ||
                        !g_liveHitQueue.empty() || g_liveIngress.depth() != 0 ||
                        !g_entityQueue.empty() || !g_recipeRpcQueue.empty() ||
                        (g_recipeRuntimeReady.load(std::memory_order_acquire) &&
                         (!g_localizationQueue.empty() || !g_localizationUnloads.empty())) ||
                        !g_dustBootstrap.empty() || g_dustWorldResetPending ||
                        g_dustMailbox.hasQueued();
                }))
                    continue;
                if (std::chrono::steady_clock::now() < until) continue;
                ++g_auditIndex;
                if (g_auditIndex >= sizeof(kAuditSamples) / sizeof(kAuditSamples[0]))
                    g_auditEnabled = false;
                lock.unlock();
                recordDomainThreads("heartbeat", seconds);
                lock.lock();
            }
            assembly = g_pinnedAssembly;
            stage = g_pinnedStage;
            g_pinnedPending = false;
        }
        ManagedRuntimeStatus result = inspectManagedRuntime();
        if (result.symbolsReady && g_api.currentThread) {
            result = stage == "PINNED_MONO"
                ? probeManagedRuntime(assembly)
                : probeManagedRuntimeStartupStage(assembly, stage);
        } else if (result.symbolsReady) {
            std::lock_guard<std::mutex> lock(g_mutex);
            result = failure("pinned-thread-api-missing",
                             "MonoVM lacks mono_thread_current");
        }
        {
            std::lock_guard<std::mutex> lock(g_pinnedMutex);
            if(result.domainInitialized&&result.methodExecuted&&result.result==42)
                g_recipeRuntimeReady.store(true,std::memory_order_release);
            g_pinnedResult = std::move(result);
            g_auditEnabled = stage == "DOMAIN_THREAD_AUDIT" && result.domainInitialized;
            g_auditIndex = 0;
            g_auditStart = std::chrono::steady_clock::now();
            g_pinnedBusy = false;
            g_pinnedResultReady = true;
        }
        g_pinnedResultWake.notify_one();
    }
    return nullptr;
}

}


void setManagedLocalizationResultSink(ManagedLocalizationResultSink sink) {
    g_localizationResultSink.store(sink, std::memory_order_release);
}

bool queueManagedLocalizationLoaded(const std::string& modId,
                                    const std::string& assemblyPath,
                                    const std::string& culture,
                                    uint64_t generation) {
    if (modId.empty() || modId.size() > 64 ||
        assemblyPath.empty() || assemblyPath.size() > 2048 ||
        culture.size() < 2 || culture.size() > 20 || generation == 0) return false;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (!g_localizationEpochFence.permits(modId, generation)) return false;
    g_localizationQueue.erase(std::remove_if(
        g_localizationQueue.begin(), g_localizationQueue.end(),
        [&](const LocalizationJob& job) {
            return job.modId == modId && job.generation < generation;
        }), g_localizationQueue.end());
    if (g_localizationQueue.size() >= 64) return false;
    g_localizationEpochFence.activate(modId, generation);
    g_localizationLiveBindings[modId] = {modId, assemblyPath, culture, generation, false, false};
    g_nextLocalizationPoll = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(250);
    g_localizationQueue.push_back({modId, assemblyPath, culture, generation, false, false});
    g_pinnedWake.notify_one();
    return true;
}

void unloadManagedLocalizationCallbacks(const std::string& modId) {
    if (modId.empty() || modId.size() > 64) return;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_localizationQueue.erase(std::remove_if(g_localizationQueue.begin(),
        g_localizationQueue.end(), [&](const LocalizationJob& job) {
            return job.modId == modId;
        }), g_localizationQueue.end());
    g_localizationEpochFence.retire(modId);
    g_localizationLiveBindings.erase(modId);
    if (std::find(g_localizationUnloads.begin(), g_localizationUnloads.end(), modId)
        == g_localizationUnloads.end())
        g_localizationUnloads.push_back(modId);
    g_pinnedWake.notify_one();
}

bool bindManagedCommandMod(const std::string& modId,
                           const std::string& assemblyPath, std::string* error) {
    if (!error) return false;
    error->clear();
    if (modId.empty() || modId.size() > 64 ||
        !validAssembly(assemblyPath)) {
        *error = "ModCommand: mod ou assembly invalidos";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_commandBindings.size() >= 128) {
        *error = "ModCommand: limite de 128 mods";
        return false;
    }
    for (const auto& binding : g_commandBindings)
        if (binding.modId == modId) {
            *error = "ModCommand: mod ja vinculado ao MonoVM";
            return false;
        }
    g_commandBindings.push_back({modId, assemblyPath});
    g_pinnedWake.notify_one();
    return true;
}

void notifyManagedCommandWorker() {
    g_pinnedWake.notify_one();
}

void resetManagedCommandSession() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_commandSessionReset = true;
    for (auto& binding : g_commandBindings) {
        binding.registered = false;
        nativeCommandMailbox().setModReady(binding.modId, false);
    }
    g_pinnedWake.notify_one();
}

bool registerManagedScreenSceneMod(const std::string& id,
                                   const std::string& assembly,
                                   const std::string& nameSpace,
                                   const std::string& className,
                                   const std::string& method) {
    auto identifier = [](const std::string& name) {
        if (name.empty() || name.size() > 72) return false;
        for (char c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) return false;
        return true;
    };
    if (id.empty() || id.size() > 128 || !validAssembly(assembly) ||
        !identifier(nameSpace) || !identifier(className) || !identifier(method))
        return false;
    std::lock_guard<std::mutex> guard(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(id)) return false;
    for (auto& binding : g_sceneLightBindings) {
        if (binding.modId != id) continue;
        if (binding.assemblyPath != assembly || binding.nameSpace != nameSpace ||
            binding.className != className || binding.methodName != method) return false;
        binding.enabled = true;
        binding.failures = 0;
        return true;
    }
    if (g_sceneLightBindings.size() >= 4) return false;
    g_sceneLightBindings.push_back({id, assembly, nameSpace, className, method});
    g_pinnedWake.notify_one();
    return true;
}

bool queueManagedScreenScene(const std::string& modId,
                             const graphics::ScreenSceneRaw& raw,
                             float baseSunX, float baseSunY, float baseClock) {
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::try_to_lock);
    if (!lock.owns_lock() || !g_pinnedStarted || g_pinnedBusy || g_pinnedPending)
        return false;
    const auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < g_sceneLightBindings.size(); ++i) {
        auto& binding = g_sceneLightBindings[i];
        if (binding.modId != modId || !binding.enabled || now < binding.nextSample)
            continue;
        binding.nextSample = now + std::chrono::milliseconds(100);
        if (!g_sceneLightQueue.empty()) {
            for (auto& job : g_sceneLightQueue)
                if (job.binding == static_cast<int>(i)) {
                    job = {static_cast<int>(i), raw, baseSunX, baseSunY,
                           baseClock, g_sceneLightGeneration};
                    return true;
                }
        }
        if (g_sceneLightQueue.size() >= 4) return false;
        g_sceneLightQueue.push_back({static_cast<int>(i), raw, baseSunX, baseSunY,
                                      baseClock, g_sceneLightGeneration});
        g_pinnedWake.notify_one();
        return true;
    }
    return false;
}

bool readManagedScreenScene(const std::string& modId, ManagedScreenLightResult* out) {
    if (!out) return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    auto it = g_sceneLightResults.find(modId);
    if (it == g_sceneLightResults.end() ||
        it->second.generation != g_sceneLightGeneration ||
        std::chrono::steady_clock::now() - it->second.updated >
            std::chrono::milliseconds(500)) return false;
    *out = it->second.result;
    return true;
}

void pauseManagedScreenSceneCallbacks() {
    std::lock_guard<std::mutex> guard(g_pinnedMutex);
    ++g_sceneLightGeneration;
    g_sceneLightQueue.clear();
    g_sceneLightResults.clear();
    for (auto& binding : g_sceneLightBindings)
        binding.nextSample = {};
}

void wakeUniversalHookWorker() { g_pinnedWake.notify_one(); }

bool registerUniversalHookMod(const std::string& id, const std::string& assembly) {
    if (id.empty() || id.size() > 128 || !validAssembly(assembly)) return false;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(id)) return false;
    for (const auto& mod : g_universalMods) if (mod.id == id) return mod.path == assembly;
    if (g_universalMods.size() >= 16) return false;
    g_universalMods.push_back({id, assembly, false});
    writeUniversalRegistrationStatus(id, "queued", "worker MonoVM aguardando registro");
    g_universalPolling.store(true, std::memory_order_release);
    g_pinnedWake.notify_one();
    return true;
}

void pauseUniversalHookMods() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_universalEnabled = false;
    g_universalUnload = true;
    for (auto& mod : g_universalMods) mod.registered = false;
    g_pinnedWake.notify_one();
}

void resumeUniversalHookMods() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_universalEnabled = true;
    if (!g_universalMods.empty()) g_universalPolling.store(true, std::memory_order_release);
    g_pinnedWake.notify_one();
}

bool registerManagedDustSourceMod(
    const std::string& modId, const std::string& assemblyPath,
    const std::vector<std::pair<int, std::string>>& dustTypes) {
    if (modId.empty() || modId.size() > 128 || !validAssembly(assemblyPath) ||
        dustTypes.empty() || dustTypes.size() > 4096) return false;
    for (const auto& [type, name] : dustTypes)
        if (type < 332 || type >= 32768 || name.empty() || name.size() > 72)
            return false;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    for (const auto& entry : g_dustBindings)
        if (entry.modId == modId)
            return entry.assemblyPath == assemblyPath && entry.types == dustTypes;
    if (g_dustBindings.size() >= 16) return false;
    for (const auto& [type, name] : dustTypes)
        if (g_dustOwners[static_cast<size_t>(type)].load(
                std::memory_order_acquire) != 0) return false;
    const int owner = static_cast<int>(g_dustBindings.size()) + 1;
    g_dustBindings.push_back({modId, assemblyPath, dustTypes, -1, true});
    for (const auto& [type, name] : dustTypes)
        g_dustOwners[static_cast<size_t>(type)].store(
            owner, std::memory_order_release);
    g_dustModCount.store(owner, std::memory_order_release);
    g_dustBootstrap.push_back(owner - 1);
    g_pinnedWake.notify_one();
    return true;
}

bool hasManagedDustType(int type) {
    return type >= 332 && type < 32768 &&
           g_dustOwners[static_cast<size_t>(type)].load(
               std::memory_order_acquire) > 0;
}

bool managedDustWorkerReady(int type) {
    if (type < 332 || type >= 32768) return false;
    const int owner = g_dustOwners[static_cast<size_t>(type)].load(
        std::memory_order_acquire) - 1;
    return owner >= 0 && static_cast<size_t>(owner) < g_dustReady.size() &&
           g_dustReady[static_cast<size_t>(owner)].load(std::memory_order_acquire);
}

bool queueManagedDustCallback(int type, int phase,
                              const ManagedDustState& state, uint64_t frame) {
    if (!hasManagedDustType(type) || type != state.type) return false;
    const int owner = g_dustOwners[static_cast<size_t>(type)].load(
        std::memory_order_acquire) - 1;
    g_dustGameFrame.store(frame, std::memory_order_release);
    bool wake = false;
    if (!g_dustMailbox.offer(owner, phase, state, frame, &wake)) return false;
    if (wake) g_pinnedWake.notify_one();
    return true;
}

bool invokeManagedDustSameFrame(int type, int phase,
                                const ManagedDustState& state, uint64_t frame,
                                std::chrono::steady_clock::time_point deadline,
                                ManagedDustMailbox::Response* response) {
    if (!response || phase < 0 || phase > 4 ||
        !hasManagedDustType(type) || type != state.type ||
        !managedDustStateValid(state) ||
        std::chrono::steady_clock::now() >= deadline)
        return false;
    const int owner = g_dustOwners[static_cast<size_t>(type)].load(
        std::memory_order_acquire) - 1;
    bool wake = false;
    if (!g_dustMailbox.offer(owner, phase, state, frame, &wake, true))
        return false;
    g_dustGameFrame.store(frame, std::memory_order_release);
    g_pinnedWake.notify_one();
    return g_dustMailbox.await(owner, phase, state, frame, deadline, response);
}

bool receiveManagedDustCallback(int type, int phase, int slot, int generation,
                                uint64_t frame, ManagedDustMailbox::Response* response) {
    if (!hasManagedDustType(type)) return false;
    const int owner = g_dustOwners[static_cast<size_t>(type)].load(
        std::memory_order_acquire) - 1;
    return g_dustMailbox.consume(owner, phase, slot, type, generation,
                                 frame, response);
}

void resetManagedDustSourceWorld() {
    g_dustGameFrame.store(0, std::memory_order_release);
    g_dustMailbox.resetWorld();
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_dustWorldResetPending = true;
    g_pinnedWake.notify_one();
}

bool registerManagedEntitySourceMod(
    const std::string& modId, const std::string& assemblyPath,
    const std::vector<std::pair<int, std::string>>& npcTypes,
    const std::vector<std::pair<int, std::string>>& projectileTypes) {
    if (modId.empty() || modId.size() > 128 || assemblyPath.empty() ||
        assemblyPath.size() > 2048 || assemblyPath.find("..") != std::string::npos ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos) return false;
    for (const auto& list : {&npcTypes, &projectileTypes})
        for (const auto& [type, name] : *list)
            if (type < 0 || type >= 32768 || name.empty() || name.size() > 128)
                return false;
    std::lock_guard<std::mutex> guard(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(modId)) return false;
    for (const auto& binding : g_entityBindings)
        if (binding.modId == modId)
            return binding.assemblyPath == assemblyPath;
    if (g_entityBindings.size() >= 16) return false;
    const int index = static_cast<int>(g_entityBindings.size());
    for (const auto& [type, unused] : npcTypes)
        if (g_entityNpcTypeOwner[static_cast<size_t>(type)].load(
                std::memory_order_acquire) != 0) return false;
    for (const auto& [type, unused] : projectileTypes)
        if (g_entityProjectileTypeOwner[static_cast<size_t>(type)].load(
                std::memory_order_acquire) != 0) return false;
    g_entityBindings.push_back({
        modId, assemblyPath, npcTypes, projectileTypes,
        -1, true, true, false, false, false, {}, 0
    });
    for (const auto& [type, unused] : npcTypes)
        g_entityNpcTypeOwner[static_cast<size_t>(type)].store(index + 1,
                                                              std::memory_order_release);
    for (const auto& [type, unused] : projectileTypes)
        g_entityProjectileTypeOwner[static_cast<size_t>(type)].store(index + 1,
                                                                     std::memory_order_release);
    g_entityModCount.store(index + 1, std::memory_order_release);
    return true;
}

bool managedEntitySourceModsRegistered() {
    return g_entityModCount.load(std::memory_order_acquire) > 0;
}

bool hasManagedEntityNpcType(int type) {
    return type >= 0 && type < 32768 &&
           g_entityNpcTypeOwner[static_cast<size_t>(type)].load(
               std::memory_order_acquire) > 0;
}

bool hasManagedEntityProjectileType(int type) {
    return type >= 0 && type < 32768 &&
           g_entityProjectileTypeOwner[static_cast<size_t>(type)].load(
               std::memory_order_acquire) > 0;
}

void pauseManagedEntitySourceCallbacks() {
    std::lock_guard<std::mutex> guard(g_pinnedMutex);
    ++g_entitySession;
    for (auto& binding : g_entityBindings) {
        binding.firstWorldErrorReported = false;
        binding.firstWorldUseReported = false;
        binding.useReports = 0;
        binding.lastUseOutcome.clear();
    }
    g_entityQueue.clear();
    g_entityNpcPending.fill(false);
    g_entityProjectilePending.fill(false);
    g_entityNpcResults.fill({});
    g_entityProjectileResults.fill({});
    g_entityNpcTicks.fill(0);
    g_entityProjectileTicks.fill(0);
}

bool queueManagedEntityCombatEvent(int eventKind, int type, int slot, int generation,
                                   int owner, uintptr_t identity, int damage, int targetIndex) {
    if (eventKind < 2 || eventKind > 5 || generation <= 0 || !identity ||
        type < 0 || type >= 32768 || slot < 0 || damage < 0 ||
        (eventKind <= 3 ? slot >= 512 : slot >= 4096) ||
        (eventKind == 4 && (targetIndex < 0 || targetIndex >= 200))) return false;
    const size_t eventIndex = static_cast<size_t>(eventKind - 2);
    const bool npc = eventKind <= 3;
    const int bound = (npc ? g_entityNpcTypeOwner[static_cast<size_t>(type)]
                           : g_entityProjectileTypeOwner[static_cast<size_t>(type)])
                          .load(std::memory_order_acquire);
    if (bound <= 0) {
        g_entityCombatUnbound[eventIndex].fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::try_to_lock);
    if (!lock.owns_lock() || !g_pinnedStarted ||
        static_cast<size_t>(bound - 1) >= g_entityBindings.size() ||
        !g_entityBindings[static_cast<size_t>(bound - 1)].armed ||
        g_entityQueue.size() >= 64) {
        g_entityCombatDropped.fetch_add(1, std::memory_order_relaxed);
        g_entityCombatDroppedByKind[eventIndex].fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    ManagedEntityJob job;
    job.binding = bound - 1;
    job.kind = eventKind;
    job.type = type;
    job.slot = slot;
    job.generation = generation;
    job.owner = owner;
    job.identity = identity;
    job.session = g_entitySession;
    job.damage = damage;
    job.target = targetIndex;
    g_entityQueue.push_back(job);
    g_entityCombatSubmitted.fetch_add(1, std::memory_order_relaxed);
    g_entityCombatQueued[eventIndex].fetch_add(1, std::memory_order_relaxed);
    g_pinnedWake.notify_one();
    return true;
}

bool pollManagedEntity(int kind, int type, int slot, int generation, int owner,
                       uintptr_t identity, int life, int lifeMax,
                       float x, float y, float vx, float vy,
                       float playerX, float playerY, ManagedEntityVelocity* velocity,
                       bool immediateFirstTick, int targetNpc,
                       float targetX, float targetY) {
    if (!velocity || generation <= 0 || identity == 0 ||
        !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(vx) || !std::isfinite(vy) ||
        !std::isfinite(playerX) || !std::isfinite(playerY) ||
        (kind == 1 && (targetNpc < -1 || targetNpc >= 200 ||
                       !std::isfinite(targetX) || !std::isfinite(targetY)))) return false;
    const int bound = kind == 0
        ? (hasManagedEntityNpcType(type)
            ? g_entityNpcTypeOwner[static_cast<size_t>(type)].load(std::memory_order_acquire) : 0)
        : (hasManagedEntityProjectileType(type)
            ? g_entityProjectileTypeOwner[static_cast<size_t>(type)].load(std::memory_order_acquire) : 0);
    if (bound == 0 || slot < 0 ||
        (kind == 0 ? static_cast<size_t>(slot) >= g_entityNpcResults.size()
                   : static_cast<size_t>(slot) >= g_entityProjectileResults.size()))
        return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::try_to_lock);
    if (!lock.owns_lock() || !g_pinnedStarted ||
        static_cast<size_t>(bound - 1) >= g_entityBindings.size() ||
        !g_entityBindings[static_cast<size_t>(bound - 1)].armed) return false;
    auto& last = kind == 0
        ? g_entityNpcResults[static_cast<size_t>(slot)]
        : g_entityProjectileResults[static_cast<size_t>(slot)];
    bool applied = false;
    if (last.ready) {
        if (last.type == type && last.generation == generation &&
            last.owner == owner && last.identity == identity &&
            last.packed >= (1LL << 32)) {
            velocity->x = static_cast<int16_t>(last.packed & 0xffff) / 256.f;
            velocity->y = static_cast<int16_t>((last.packed >> 16) & 0xffff) / 256.f;
            applied = std::isfinite(velocity->x) && std::isfinite(velocity->y);
        }
        last = {};
    }
    auto& pending = kind == 0
        ? g_entityNpcPending[static_cast<size_t>(slot)]
        : g_entityProjectilePending[static_cast<size_t>(slot)];
    auto& ticks = kind == 0
        ? g_entityNpcTicks[static_cast<size_t>(slot)]
        : g_entityProjectileTicks[static_cast<size_t>(slot)];
    ++ticks;
    if ((!immediateFirstTick && ticks % 6 != 0) || pending) return applied;
    if (g_entityQueue.size() >= 32) {
        g_entityJobsDropped.fetch_add(1, std::memory_order_relaxed);
        return applied;
    }
    g_entityQueue.push_back({
        bound - 1, kind, type, slot, generation, owner, life, lifeMax,
        identity, x, y, vx, vy, playerX, playerY, g_entitySession
    });
    if (kind == 1) {
        g_entityQueue.back().target = targetNpc;
        g_entityQueue.back().targetX = targetX;
        g_entityQueue.back().targetY = targetY;
    }
    pending = true;
    g_entityJobsSubmitted.fetch_add(1, std::memory_order_relaxed);
    g_pinnedWake.notify_one();
    return applied;
}

bool tickManagedEntityNpc(int type, int slot, int generation,
                          uintptr_t identity, int life, int lifeMax,
                          float x, float y, float vx, float vy,
                          float playerX, float playerY, ManagedEntityVelocity* result) {
    return pollManagedEntity(0, type, slot, generation, -1, identity, life, lifeMax,
                             x, y, vx, vy, playerX, playerY, result, false, -1, 0.f, 0.f);
}

bool tickManagedEntityProjectile(int type, int slot, int generation, int owner,
                                 uintptr_t identity, float x, float y,
                                 float vx, float vy, float playerX, float playerY,
                                 ManagedEntityVelocity* result, bool immediateFirstTick,
                                 int targetNpc, float targetX, float targetY) {
    return pollManagedEntity(1, type, slot, generation, owner, identity, 0, 0,
                             x, y, vx, vy, playerX, playerY, result, immediateFirstTick,
                             targetNpc, targetX, targetY);
}

void scheduleManagedInvasionWorldTicks(uint64_t worldSession, int mode,
                                       int playerIndex, float playerX, float playerY) {
    if (worldSession == 0 || !managedEntitySourceModsRegistered() ||
        (mode < 0 || mode > 3) ||
        playerIndex < 0 || !std::isfinite(playerX) || !std::isfinite(playerY)) return;
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::try_to_lock);
    if (!lock.owns_lock() || !g_pinnedStarted || g_entityQueue.size() >= 16) return;
    const int bindings = g_entityModCount.load(std::memory_order_acquire);
    for (int index = 0; index < bindings; ++index) {
        if (static_cast<size_t>(index) >= g_entityBindings.size() ||
            g_entityQueue.size() >= 16) break;
        if (!g_entityBindings[static_cast<size_t>(index)].armed ||
            !g_entityBindings[static_cast<size_t>(index)].invasionSupported)
            continue;
        ManagedEntityJob job;
        job.binding = index;
        job.kind = 6;
        job.owner = mode;
        job.life = 30;
        job.identity = static_cast<uintptr_t>(worldSession);
        job.target = playerIndex;
        job.x = playerX;
        job.y = playerY;
        job.session = g_entitySession;
        g_entityQueue.push_back(std::move(job));
    }
    g_pinnedWake.notify_one();
}

bool registerManagedPlayerHurtMod(const std::string& modId,
                                  const std::string& assemblyPath) {
    if (modId.empty() || modId.size() > 128 || assemblyPath.empty() ||
        assemblyPath.size() > 2048 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        assemblyPath.find("..") != std::string::npos)
        return false;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(modId)) return false;
    for (auto& entry : g_managedHurtBindings) {
        if (entry.modId != modId) continue;
        if (entry.assemblyPath != assemblyPath) return false;
        entry.active = true;
        return true;
    }
    if (g_managedHurtBindings.size() >= 16 || g_managedHurtQueue.size() >= 32) return false;
    const int index = static_cast<int>(g_managedHurtBindings.size());
    g_managedHurtBindings.push_back({modId, assemblyPath, -1, true, false});
    g_managedHurtQueue.push_back(ManagedHurtJob{
        0, index, true, 0, 0, 0, 0, 0, 0, 0
    });
    g_managedHurtQueue.back().generation = g_managedHurtGeneration;
    g_pinnedWake.notify_one();
    return true;
}

void setManagedHurtDiagnosticLogger(ManagedHurtDiagnosticLogFn logger) {
    g_hurtDiagnosticLogger.store(logger, std::memory_order_release);
}

void pauseManagedPlayerHurtMods() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    ++g_managedHurtGeneration;
    for (auto& entry : g_managedHurtBindings) entry.active = false;
    for (auto& job : g_managedHurtQueue) {
        if (!job.completion) continue;
        job.completion->cancelled = job.completion->done = true;
        ++g_hurtScheduler.cancelledBeforeRun;
    }
    g_managedHurtQueue.clear();
    g_managedHurtResultWake.notify_all();
}

ManagedHurtSchedulerStatus managedHurtSchedulerStatus() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    auto status = g_hurtScheduler;
    status.pending = static_cast<int>(g_managedHurtQueue.size());
    return status;
}

bool managedRecipeRuntimeReady(){return g_recipeRuntimeReady.load(std::memory_order_acquire);}

static bool invokeManagedCallback(bool shooting,const std::string& mod,const std::string& assembly,
                         const std::string& owner,int index,int kind,const std::string& snapshot,
                         std::string* output,int budgetMicroseconds,bool tile=false,bool rarity=false) {
    if(!output)return false;
    if(budgetMicroseconds<1||budgetMicroseconds>3000||index<0||index>=16||kind<0||kind>6||
       snapshot.size()>(rarity?32768u:(shooting?1024u:512u))||assembly.empty()||assembly.size()>4096){*output="ERROR|Stage=SubmitRecipeCallback Reason=InvalidTransportArguments";return false;}
    std::unique_lock<std::mutex> lock(g_pinnedMutex,std::try_to_lock);
    if(!lock.owns_lock()){*output="PENDING|Stage=SubmitRecipeCallback Reason=WorkerQueueBusy";return false;}
    if(!g_pinnedStarted){*output="PENDING|Stage=SubmitRecipeCallback Reason=WorkerNotStarted";return false;}
    if(g_recipeRpcQueue.size()>=16){*output="PENDING|Stage=SubmitRecipeCallback Reason=QueueCapacityExceeded Expected=Below16";return false;}
    const auto until=std::chrono::steady_clock::now()+std::chrono::microseconds(budgetMicroseconds);
    auto completion=std::make_shared<RecipeRpcCompletion>();
    g_recipeRpcQueue.push_back({mod,assembly,owner,snapshot,index,kind,until,completion,shooting,tile,rarity});g_pinnedWake.notify_one();
    if(!g_recipeRpcWake.wait_until(lock,until,[&]{return completion->done;})) {
        completion->cancelled=true;*output="PENDING|Stage=WaitRecipeCallback Reason=DeadlineExceeded BudgetUs="+std::to_string(budgetMicroseconds);return false;
    }
    *output=std::move(completion->output);return !output->empty();
}

bool invokeManagedRecipe(const std::string& mod,const std::string& assembly,
    const std::string& owner,int index,int kind,const std::string& snapshot,std::string* output,int budgetMicroseconds) {
    return invokeManagedCallback(false,mod,assembly,owner,index,kind,snapshot,output,budgetMicroseconds);
}
bool invokeManagedShoot(const std::string& mod,const std::string& assembly,const std::string& owner,
    int kind,const std::string& snapshot,std::string* output) {
    if(kind<0||kind>1){if(output)*output="ERROR|Reason=InvalidShootCallbackKind";return false;}
    const bool result=invokeManagedCallback(true,mod,assembly,owner,0,kind,snapshot,output,3000);
    if(output) { // Preserve the actual cause while identifying the correct API in transport diagnostics.
        size_t pos=0;while((pos=output->find("RecipeCallback",pos))!=std::string::npos){output->replace(pos,14,"ShootCallback");pos+=13;}
    }
    return result;
}
bool invokeManagedTile(const std::string& mod,const std::string& assembly,
    const std::string& owner,int callback,int x,int y,std::string* output,int budgetMicroseconds) {
    if(callback<0||callback>12||x<0||y<0||x>100000||y>100000) {
        if(output)*output="ERROR|Stage=TileCallback Reason=InvalidCoordinatesOrKind";
        return false;
    }
    return invokeManagedCallback(false,mod,assembly,owner,callback,0,
        std::to_string(x)+","+std::to_string(y),output,budgetMicroseconds,true);
}
bool invokeManagedTileDraw(const std::string& mod,const std::string& assembly,
    const std::string& owner,const std::string& snapshot,
    std::string* output,int budgetMicroseconds) {
    if(snapshot.size()>512||snapshot.empty()) {
        if (output) *output = "ERROR|Stage=TileDraw Reason=InvalidSnapshot";
        return false;
    }
    return invokeManagedCallback(false,mod,assembly,owner,13,0,
        snapshot,output,budgetMicroseconds,true);
}
bool queueManagedRarityWarmup(const std::string& mod,const std::string& assembly,
    const std::string& identities) {
    if(mod.empty()||mod.size()>96||assembly.empty()||assembly.size()>4096||
       identities.empty()||identities.size()>32768)return false;
    std::lock_guard lock(g_pinnedMutex);
    const auto key=mod+"|"+assembly;
    if(g_rarityWarmups.count(key))return false;
    if(g_rarityWarmups.size()>=4096||g_recipeRpcQueue.size()>=4096)return false;
    auto completion=std::make_shared<RecipeRpcCompletion>();
    g_rarityWarmups.emplace(key,completion);
    g_recipeRpcQueue.push_back({mod,assembly,"",identities,0,2,
        std::chrono::steady_clock::now()+std::chrono::seconds(60),completion,false,false,true});
    g_pinnedWake.notify_one();
    return true;
}
int managedRarityWarmupState(const std::string& mod,const std::string& assembly,
    std::string* error) {
    std::unique_lock lock(g_pinnedMutex,std::try_to_lock);
    if(!lock.owns_lock())return 0;
    const auto found=g_rarityWarmups.find(mod+"|"+assembly);
    if(found==g_rarityWarmups.end()){
        if(error)*error="Stage=Warmup Reason=RarityModNotQueued";
        return -1;
    }
    if(!found->second->done)return 0;
    if(found->second->output.starts_with("READY|"))return 1;
    if(error)*error=found->second->output;
    return -1;
}
bool invokeManagedRarity(const std::string& mod,const std::string& assembly,
    const std::string& owner,int kind,const std::string& snapshot,
    std::string* output,int budgetMicroseconds) {
    if(owner.empty()||owner.size()>72||kind<0||kind>1) {
        if(output)*output="ERROR|Stage=RarityCallback Reason=InvalidOwnerOrKind";
        return false;
    }
    return invokeManagedCallback(false,mod,assembly,owner,0,kind,
        snapshot,output,budgetMicroseconds,false,true);
}
bool queueManagedTileEvent(const std::string& mod,const std::string& assembly,
    const std::string& owner,int callback,int x,int y) {
    if(callback<2||callback>12||x<0||y<0||x>100000||y>100000||assembly.empty())return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex,std::try_to_lock);
    if(!lock.owns_lock()||!g_pinnedStarted||g_recipeRpcQueue.size()>=16)return false;
    auto result=std::make_shared<RecipeRpcCompletion>();
    g_recipeRpcQueue.push_back({mod,assembly,owner,std::to_string(x)+","+std::to_string(y),
        callback,0,std::chrono::steady_clock::now()+std::chrono::milliseconds(100),result,false,true});
    g_pinnedWake.notify_one();return true;
}
bool queueManagedTileNearby(const std::string& mod,const std::string& assembly,
    const std::string& owner,int x,int y,bool closer) {
    if(x<0||y<0||x>100000||y>100000||assembly.empty())return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex,std::try_to_lock);
    if(!lock.owns_lock()||!g_pinnedStarted||g_recipeRpcQueue.size()>=16)return false;
    auto result=std::make_shared<RecipeRpcCompletion>();
    g_recipeRpcQueue.push_back({mod,assembly,owner,
        std::to_string(x)+","+std::to_string(y)+","+(closer?"1":"0"),
        11,0,std::chrono::steady_clock::now()+std::chrono::milliseconds(100),
        result,false,true});
    g_pinnedWake.notify_one();return true;
}
bool queueManagedTileWarmup(const std::string& mod,const std::string& assembly,const std::string& owner) {
    if(mod.empty()||owner.empty()||assembly.empty()||assembly.size()>4096)return false;
    std::lock_guard lock(g_pinnedMutex);
    const auto key=mod+"|"+assembly+"|"+owner;
    if(g_tileWarmups.count(key))return true;
    if(g_tileWarmups.size()>=4096||g_recipeRpcQueue.size()>=4096)return false;
    auto result=std::make_shared<RecipeRpcCompletion>();
    g_tileWarmups.emplace(key,result);
    g_recipeRpcQueue.push_back({mod,assembly,owner,"",0,2,
        std::chrono::steady_clock::now()+std::chrono::seconds(60),result,false,true});
    g_pinnedWake.notify_one();return true;
}
int managedTileWarmupState(const std::string& mod,const std::string& assembly,
    const std::string& owner,std::string* error) {
    std::unique_lock lock(g_pinnedMutex,std::try_to_lock);
    if(!lock.owns_lock())return 0;
    const auto key=mod+"|"+assembly+"|"+owner;
    const auto found=g_tileWarmups.find(key);
    if(found==g_tileWarmups.end()){if(error)*error="Stage=Warmup Reason=TileOwnerNotQueued";return -1;}
    if(!found->second->done)return 0;
    if(found->second->output=="READY")return 1;
    if(error)*error=found->second->output;
    return -1;
}
static std::string shootWarmupKey(const std::string& mod,const std::string& assembly,const std::string& owner) {
    return mod+"|"+assembly+"|"+owner;
}
bool queueManagedShootWarmup(const std::string& mod,const std::string& assembly,const std::string& owner) {
    if(mod.empty()||owner.empty()||assembly.empty()||assembly.size()>4096)return false;
    std::lock_guard lock(g_pinnedMutex);const auto key=shootWarmupKey(mod,assembly,owner);
    if(g_shootWarmups.count(key))return true;
    if(g_shootWarmups.size()>=4096||g_recipeRpcQueue.size()>=4096)return false;
    auto completion=std::make_shared<RecipeRpcCompletion>();
    g_shootWarmups.emplace(key,completion);
    g_recipeRpcQueue.push_back({mod,assembly,owner,"",0,2,std::chrono::steady_clock::now()+std::chrono::seconds(60),completion,true});
    g_pinnedWake.notify_one();return true;
}
int managedShootWarmupState(const std::string& mod,const std::string& assembly,const std::string& owner,std::string* error) {
    std::unique_lock lock(g_pinnedMutex,std::try_to_lock);if(!lock.owns_lock())return 0;
    const auto found=g_shootWarmups.find(shootWarmupKey(mod,assembly,owner));
    if(found==g_shootWarmups.end()){*error="Stage=Warmup Reason=OwnerNotQueued";return -1;}
    if(!found->second->done)return 0;
    if(found->second->output=="READY")return 1;
    *error=found->second->output;return -1;
}

bool modifyManagedPlayerHurt(const std::string& modId, int damage,
                             int hitDirection, int flags, int life, int lifeMax,
                             int equipmentMask, int npcIndex, int projectileIndex,
                             int npcType, int npcDamage, int npcLife,
                             int projectileType, int projectileDamage, int projectileOwner,
                             float attackerX, float attackerY,
                             float attackerVelocityX, float attackerVelocityY,
                             float attackerAngle, float attackerDistance,
                             ManagedHurtResult* result) {
    if (!result || damage < 0 || damage > 1000000 || life < 0 ||
        lifeMax < 1 || equipmentMask < 0 || hitDirection < -1 ||
        hitDirection > 1)
        return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::defer_lock);
    if (!lock.try_lock() || !g_pinnedStarted || g_managedHurtQueue.size() >= 8) return false;
    int index = -1;
    for (size_t i = 0; i < g_managedHurtBindings.size(); ++i) {
        if (g_managedHurtBindings[i].modId == modId &&
            g_managedHurtBindings[i].active &&
            g_managedHurtBindings[i].managedSlot >= 0) {
            index = static_cast<int>(i);
            break;
        }
    }
    if (index < 0) return false;
    const auto started = std::chrono::steady_clock::now();
    auto completion = std::make_shared<ManagedHurtCompletion>();
    completion->deadline = started + std::chrono::milliseconds(5);
    const uint64_t ticket = ++g_managedHurtTicket;
    g_managedHurtQueue.push_back({ticket, index, false, damage, 0,
                                  hitDirection, flags, life, lifeMax, equipmentMask,
                                  npcIndex, projectileIndex,
                                  npcType, npcDamage, npcLife,
                                  projectileType, projectileDamage, projectileOwner,
                                  attackerX, attackerY, attackerVelocityX, attackerVelocityY,
                                  attackerAngle, attackerDistance});
    g_managedHurtQueue.back().completion = completion;
    g_managedHurtQueue.back().generation = g_managedHurtGeneration;
    ++g_hurtScheduler.submitted;
    g_pinnedWake.notify_one();
    const bool completed = g_managedHurtResultWake.wait_until(
        lock, completion->deadline, [&completion] {
            return completion->done;
        });
    const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count());
    g_hurtScheduler.waitTotalUs += elapsed;
    g_hurtScheduler.waitMaxUs = std::max(g_hurtScheduler.waitMaxUs, elapsed);
    if (!completed || completion->expired) {
        completion->cancelled = true;
        ++g_hurtScheduler.timedOut;
        auto pending = std::find_if(g_managedHurtQueue.begin(), g_managedHurtQueue.end(),
            [ticket](const auto& job) { return job.ticket == ticket; });
        if (pending != g_managedHurtQueue.end()) {
            g_managedHurtQueue.erase(pending);
            ++g_hurtScheduler.cancelledBeforeRun;
        }
        return false;
    }
    if (completion->cancelled || completion->value < 0)
        return false;
    const uint64_t packed = static_cast<uint64_t>(completion->value);
    const int newDamage = static_cast<int>(packed & 0xfffff);
    const int newDirection = static_cast<int>((packed >> 20) & 3) - 1;
    const int newFlags = static_cast<int>((packed >> 22) & 63);
    if (newDamage < 0 || newDamage > 1000000 ||
        newDirection < -1 || newDirection > 1) return false;
    *result = {newDamage, newDirection, newFlags & 15,
               (newFlags & 16) != 0, (newFlags & 32) != 0};
    return true;
}

bool notifyManagedPlayerHurt(const std::string& modId, int sourceDamage,
                             int actualDamage, int hitDirection, int flags,
                             int lifeRemaining, int equipmentMask,
                             int npcIndex, int projectileIndex,
                             int npcType, int npcDamage, int npcLife,
                             int projectileType, int projectileDamage, int projectileOwner,
                             float attackerX, float attackerY,
                             float attackerVelocityX, float attackerVelocityY,
                             float attackerAngle, float attackerDistance) {
    if (actualDamage <= 0 || sourceDamage < 0 || actualDamage > 1000000 ||
        lifeRemaining < 0) return false;
    std::unique_lock<std::mutex> lock(g_pinnedMutex, std::defer_lock);
    if (!lock.try_lock() || !g_pinnedStarted || g_managedHurtQueue.size() >= 32)
        return false;
    for (size_t i = 0; i < g_managedHurtBindings.size(); ++i) {
        const auto& entry = g_managedHurtBindings[i];
        if (entry.modId != modId || !entry.active || entry.managedSlot < 0)
            continue;
        g_managedHurtQueue.push_back({
            0, static_cast<int>(i), true, actualDamage, sourceDamage,
            hitDirection, flags, lifeRemaining, 0, equipmentMask,
            npcIndex, projectileIndex,
            npcType, npcDamage, npcLife,
            projectileType, projectileDamage, projectileOwner,
            attackerX, attackerY, attackerVelocityX, attackerVelocityY,
            attackerAngle, attackerDistance
        });
        g_managedHurtQueue.back().generation = g_managedHurtGeneration;
        g_pinnedWake.notify_one();
        return true;
    }
    return false;
}

void setManagedCheckpointPath(const std::string& path) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (path.size() <= 2048 && path.find("..") == std::string::npos)
        g_checkpointPath = path;
}

bool invokeInteractiveCSharp(const std::string& assemblyPath, std::string* output, std::string* error) {
    if (!output || !error) return false;
    output->clear();
    error->clear();
    if (!validAssembly(assemblyPath)) {
        *error="Console assembly missing or invalid";
        return false;
    }
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_domain || !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke) {
        *error="MonoVM domain not ready";
        return false;
    }
    if (!g_api.attach(g_domain)) {
        *error="MonoVM game thread attach failed";
        return false;
    }
    void* assembly=g_api.openAssembly(g_domain,assemblyPath.c_str());
    void* image=assembly?g_api.image(assembly):nullptr;
    void* klass=image?g_api.findClass(image,"BunnyLoader.Interactive","ConsoleEntry"):nullptr;
    void* method=klass?g_api.findMethod(klass,"Run",0):nullptr;
    if (!method) {
        *error="BunnyLoader.Interactive.ConsoleEntry.Run() not found";
        return false;
    }
    void* exception=nullptr;
    void* result=g_api.invoke(method,nullptr,nullptr,&exception);
    auto toUtf8=resolve<char* (*)(void*)>("mono_string_to_utf8");
    auto release=resolve<void (*)(void*)>("mono_free");
    if (exception) {
        auto toString=resolve<void* (*)(void*,void**)>("mono_object_to_string");
        void* renderError=nullptr;
        void* message=toString?toString(exception,&renderError):nullptr;
        if (message && !renderError && toUtf8 && release) {
            char* text=toUtf8(message);
            if (text){*error=std::string(text).substr(0,4096);release(text);}
        }
        if (error->empty())*error="Managed CSharp code threw an exception";
        return false;
    }
    if (!result || !toUtf8 || !release) {
        *error="MonoVM console result unavailable";
        return false;
    }
    char* text=toUtf8(result);
    if (!text) {
        *error="MonoVM console UTF8 conversion failed";
        return false;
    }
    *output=std::string(text).substr(0,8192);
    release(text);
    return true;
}

std::string managedHurtHookSnapshot() {
    std::lock_guard<std::mutex> guard(g_hurtSnapshotMutex);
    return g_hurtSnapshotText;
}

ManagedRuntimeStatus managedRuntimeStatus() {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_status;
}

ManagedRuntimeStatus inspectManagedRuntime() {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.attempted = true;
    g_status.error.clear();
    if (loadLibrary() && !g_status.domainInitialized)
        trace("symbols-ready");
    return g_status;
}

ManagedRuntimeStatus probeManagedRuntime(const std::string& assemblyPath) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.attempted = true;
    g_status.methodExecuted = false;
    g_status.result = 0;
    g_status.error.clear();
    trace("checking-assembly");
    if (!validAssembly(assemblyPath))
        return failure("invalid-assembly", "assembly de teste ausente ou fora do limite");

    if (!loadLibrary()) return g_status;

    if (!g_domain) {
        const auto slash = assemblyPath.find_last_of('/');
        if (g_api.setAssembliesPath && slash != std::string::npos) {
            trace("setting-assembly-path");
            g_api.setAssembliesPath(assemblyPath.substr(0, slash).c_str());
        }
#ifdef __ANDROID__
        trace("checking-system-native");
        void* systemNative = dlopen("libSystem.Native.so", RTLD_NOW | RTLD_LOCAL);
        if (!systemNative) {
            const char* message = dlerror();
            return failure("system-native-missing",
                           message ? message : "libSystem.Native.so indisponivel");
        }
        trace("system-native-ready");
#endif
        auto signalPolicy = reinterpret_cast<int (*)()>(
            dlsym(g_library, "mono_bunny_android_signal_policy_v1"));
        const bool alternativeSignals = signalPolicy && signalPolicy() == 1;
        const auto conflict = monoAndroidSignalConflict(alternativeSignals);
        if (!conflict.empty()) return failure("mono-signal-conflict", conflict);
        trace("initializing-domain");
        g_domain = g_api.initialize("BunnyCSharp");
        if (!g_domain)
            return failure("domain-failed", "mono_jit_init retornou nulo");
    }
    g_status.domainInitialized = true;
    trace("attaching-thread");
    if (!g_api.attach(g_domain))
        return failure("attach-failed", "mono_thread_attach retornou nulo");

    trace("opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
    if (!assembly) return failure("assembly-failed", "mono_domain_assembly_open falhou");

    trace("resolving-method");
    void* image = g_api.image(assembly);
    void* klass = image ? g_api.findClass(image, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "Run", 2) : nullptr;
    if (!method) return failure("method-missing", "BunnyLoader.ManagedProbe.Probe.Run(int,int) ausente");

    int a = 19;
    int b = 23;
    void* parameters[] = {&a, &b};
    void* exception = nullptr;
    trace("invoking-method");
    void* result = g_api.invoke(method, nullptr, parameters, &exception);
    if (exception) return failure("managed-exception", "Run lancou uma excecao gerenciada");
    if (!result) return failure("null-result", "Run nao retornou objeto");

    void* unboxed = g_api.unbox(result);
    if (!unboxed) return failure("unbox-failed", "mono_object_unbox retornou nulo");

    int value = 0;
    std::memcpy(&value, unboxed, sizeof(value));
    g_status.result = value;
    if (value != 42) return failure("result-mismatch", "Run retornou resultado inesperado");

    g_status.methodExecuted = true;
    trace("verified");
    return g_status;
}


ManagedRuntimeStatus probeManagedRuntimeStartupStage(
    const std::string& assemblyPath, const std::string& stage) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.attempted = true;
    g_status.methodExecuted = false;
    g_status.result = 0;
    g_status.error.clear();
    trace("stage-checking-assembly");
    if (stage != "LIBRARY_ONLY" && stage != "PATH_ONLY" &&
        stage != "SYSTEM_NATIVE_ONLY" && stage != "DOMAIN_ONLY" &&
        stage != "ATTACH_ONLY" && stage != "DOMAIN_THREAD_AUDIT")
        return failure("stage-invalid", "Unsupported staged MonoVM probe");
    if (!validAssembly(assemblyPath))
        return failure("stage-assembly-invalid", "Managed probe DLL missing");
    if (!loadLibrary()) return g_status;
    if (stage == "LIBRARY_ONLY") {
        trace("library-only-verified");
        return g_status;
    }
    if (!g_domain) {
        const auto slash = assemblyPath.find_last_of('/');
        if (g_api.setAssembliesPath && slash != std::string::npos) {
            trace("stage-setting-assembly-path");
            g_api.setAssembliesPath(assemblyPath.substr(0, slash).c_str());
        }
        if (stage == "PATH_ONLY") {
            trace("path-only-verified");
            return g_status;
        }
#ifdef __ANDROID__
        trace("stage-checking-system-native");
        void* systemNative = dlopen("libSystem.Native.so", RTLD_NOW | RTLD_LOCAL);
        if (!systemNative) {
            const char* message = dlerror();
            return failure("stage-system-native-missing",
                           message ? message : "libSystem.Native.so unavailable");
        }
        trace("stage-system-native-ready");
#endif
        if (stage == "SYSTEM_NATIVE_ONLY") {
            trace("system-native-only-verified");
            return g_status;
        }
        if (stage == "DOMAIN_THREAD_AUDIT")
            recordDomainThreads("before-mono-jit-init", -1, true);
        auto signalPolicy = reinterpret_cast<int (*)()>(
            dlsym(g_library, "mono_bunny_android_signal_policy_v1"));
        const bool alternativeSignals = signalPolicy && signalPolicy() == 1;
        const auto conflict = monoAndroidSignalConflict(alternativeSignals);
        if (!conflict.empty()) return failure("mono-signal-conflict", conflict);
        trace("stage-initializing-domain");
        g_domain = g_api.initialize("BunnyCSharp");
        if (!g_domain)
            return failure("stage-domain-failed", "mono_jit_init returned null");
        if (stage == "DOMAIN_THREAD_AUDIT")
            recordDomainThreads("after-mono-jit-init");
    }
    g_status.domainInitialized = true;
    if (stage == "DOMAIN_THREAD_AUDIT") {
        trace("domain-thread-audit-running");
        return g_status;
    }
    if (stage == "DOMAIN_ONLY") {
        trace("domain-only-verified");
        return g_status;
    }
    trace("stage-attaching-thread");
    if (!g_api.attach(g_domain))
        return failure("stage-attach-failed", "mono_thread_attach returned null");
    trace("attach-only-verified");
    return g_status;
}

ManagedRuntimeStatus probeManagedRuntimePinnedThread(
    const std::string& assemblyPath, const std::string& stage) {
    std::unique_lock<std::mutex> lock(g_pinnedMutex);
    if (!g_pinnedStarted) {
        pthread_t thread{};
        const int result = pthread_create(&thread, nullptr, &pinnedMonoWorker, nullptr);
        if (result != 0) {
            ManagedRuntimeStatus failed;
            failed.attempted = true;
            failed.phase = "pinned-worker-create-failed";
            failed.error = "pthread_create failed: " + std::to_string(result);
            return failed;
        }
        pthread_detach(thread);
        g_pinnedStarted = true;
    }
    if (g_pinnedBusy) {
        ManagedRuntimeStatus failed;
        failed.attempted = true;
        failed.phase = "pinned-worker-busy";
        failed.error = "Pinned MonoVM worker is already running";
        return failed;
    }
    g_pinnedBusy = true;
    g_pinnedResultReady = false;
    g_pinnedAssembly = assemblyPath;
    g_pinnedStage = stage;
    g_pinnedPending = true;
    g_pinnedWake.notify_one();
    g_pinnedResultWake.wait(lock, [] { return g_pinnedResultReady; });
    return g_pinnedResult;
}

bool enableManagedLiveHit(const std::string& modId,
                          const std::string& assemblyPath,
                          int projectileType, std::string* error, bool automatic) {
    if (!error) return false;
    error->clear();
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        projectileType < 0 || projectileType >= 32768 ||
        !validAssembly(assemblyPath)) {
        *error = "C# mod, assembly ou tipo do projetil invalidos";
        return false;
    }
    const ManagedRuntimeStatus state = managedRuntimeStatus();
    if (!state.domainInitialized || !state.methodExecuted) {
        *error = "Execute managedGameProbe PINNED_MONO antes de ativar o evento";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(modId)) {
        *error = "Live Mono callbacks were revoked; restart Bunny Loader";
        return false;
    }
    if (!g_pinnedStarted || g_pinnedBusy || g_liveBenchmarkAttempted ||
        g_liveHitInFlight || !g_liveHitQueue.empty() ||
        g_liveIngress.depth() != 0) {
        *error = "MonoVM owner ocupado ou indisponivel";
        return false;
    }
    g_liveHitType.store(-1, std::memory_order_release);
    disarmAdditionalLiveBindings();
    g_additionalLiveCount.store(0, std::memory_order_release);
    for (auto& slot : g_additionalLiveByType)
        slot.store(0, std::memory_order_relaxed);
    ++g_liveHitGeneration;
    g_buffEffectGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_buffRequested.store(0, std::memory_order_relaxed);
    g_buffApplied.store(0, std::memory_order_relaxed);
    g_buffRejected.store(0, std::memory_order_relaxed);
    g_buffStale.store(0, std::memory_order_relaxed);
    g_buffQueueDrops.store(0, std::memory_order_relaxed);
    g_buffLastId.store(0, std::memory_order_relaxed);
    g_buffLastDuration.store(0, std::memory_order_relaxed);
    g_buffLastNpc.store(-1, std::memory_order_relaxed);
    g_impulsesApplied.store(0, std::memory_order_relaxed);
    g_lastImpulseDirection.store(0, std::memory_order_relaxed);
    g_lastImpulseTenths.store(0, std::memory_order_relaxed);
    g_liveHitQueue.clear();
    g_liveIngress.reset(automatic);
    g_liveHitLockDrops.store(0, std::memory_order_relaxed);
    g_liveHitStatus = {};
    g_primaryLiveFailures = 0;
    g_liveHitStatus.automatic = automatic;
    g_liveHitStatus.eventLimit = automatic ? 0 : ManagedLiveEventQueue::kEventLimit;
    g_liveHitStatus.perBindingQueueCapacity = automatic ? ManagedFairEventQueue::kPerBindingCapacity : ManagedLiveEventQueue::kCapacity;
    g_liveHitStatus.modId = modId;
    g_liveHitStatus.projectileType = projectileType;
    g_liveHitStatus.enabled = true;
    g_liveHitStatus.phase = "armed";
    g_liveHitAssembly = assemblyPath;
    g_liveHitAutomatic.store(automatic, std::memory_order_release);
    g_liveHitType.store(projectileType, std::memory_order_release);
    return true;
}

ManagedMemoryStatus managedMemoryStatus(bool resetBaseline) {
    ManagedMemoryStatus status;
    const ManagedProcMemory proc = readManagedProcMemory();
    status.available = proc.available;
    status.rssKb = proc.rssKb;
    status.peakRssKb = proc.peakRssKb;
    status.virtualKb = proc.virtualKb;
    status.dataKb = proc.dataKb;
    status.swapKb = proc.swapKb;
    status.threads = proc.threads;
    {
        std::lock_guard<std::mutex> lock(g_memoryBaselineMutex);
        if (proc.available && (resetBaseline || g_memoryBaselineRssKb == 0))
            g_memoryBaselineRssKb = proc.rssKb;
        status.baselineRssKb = g_memoryBaselineRssKb;
        if (proc.available)
            status.rssDeltaKb = proc.rssKb - g_memoryBaselineRssKb;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        status.monoReady = g_domain && g_status.domainInitialized &&
                           g_status.methodExecuted;
        status.cachedCallbackEntries =
            static_cast<int>(g_sourceLiveEntries.size());
        std::set<std::string> paths;
        for (const auto& [key, entry] : g_sourceLiveEntries)
            if (!entry.path.empty()) paths.insert(entry.path);
        status.cachedAssemblyPaths = static_cast<int>(paths.size());
        status.monoGcStatsAvailable = status.monoReady &&
            g_api.gcHeapSize && g_api.gcUsedSize;
        if (status.monoGcStatsAvailable) {
            status.monoGcHeapBytes = g_api.gcHeapSize();
            status.monoGcUsedBytes = g_api.gcUsedSize();
            if (status.monoGcHeapBytes < 0 || status.monoGcUsedBytes < 0) {
                status.monoGcStatsAvailable = false;
                status.monoGcHeapBytes = 0;
                status.monoGcUsedBytes = 0;
            }
        }
        status.liveMethodCacheHits = g_liveMethodCacheHits;
        status.liveMethodCacheMisses = g_liveMethodCacheMisses;
        status.liveAssemblyOpenAttempts = g_liveAssemblyOpenAttempts;
        status.liveAssemblyOpenSuccesses = g_liveAssemblyOpenSuccesses;
        status.distinctLiveOpenedAssemblyPaths =
            static_cast<int>(g_liveAssemblyOpenPaths.size());
    }
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        status.workerTid = g_pinnedTid;
        status.boundProjectileTypes =
            (g_liveHitStatus.enabled ? 1 : 0) +
            g_additionalLiveCount.load(std::memory_order_acquire);
        status.activeProjectileTypes = g_liveHitStatus.enabled ? 1 : 0;
        const int count = g_additionalLiveCount.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) {
            const auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
            if (entry.type == entry.armedType.load(std::memory_order_acquire))
                ++status.activeProjectileTypes;
        }
    }
    return status;
}

ManagedLiveHitStatus managedLiveHitStatus() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    ManagedLiveHitStatus status = g_liveHitStatus;
    status.primaryEnabled = status.enabled &&
        g_liveHitType.load(std::memory_order_acquire) == status.projectileType;
    status.primaryQueueDrops = g_liveIngress.bindingDrops(-1);
    status.queued = std::max(status.queued, g_liveIngress.accepted());
    status.queueDepth = g_liveHitQueue.size() + g_liveIngress.depth();
    status.maxQueueDepth = std::max(status.maxQueueDepth, g_liveIngress.maxDepth());
    status.inFlight = g_liveHitInFlight;
    status.lockDrops = g_liveHitLockDrops.load(std::memory_order_relaxed);
    status.effects = managedNpcBuffEffectStatus();
    addManagedCounter(status.dropped, status.lockDrops);
    addManagedCounter(status.dropped, g_liveIngress.dropped());
    return status;
}

ManagedLiveHitStatus pauseManagedLiveHit() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_liveBenchmark.running) {
        finishBenchmarkErrorLocked("benchmark-cancelled", "Callback pausado pelo usuario");
        return g_liveHitStatus;
    }
    g_liveHitType.store(-1, std::memory_order_release);
    disarmAdditionalLiveBindings();
    g_liveHitAutomatic.store(false, std::memory_order_release);
    ++g_liveHitGeneration;
    g_buffEffectGeneration.fetch_add(1, std::memory_order_acq_rel);
    g_liveHitStatus.queued = std::max(
        g_liveHitStatus.queued, g_liveIngress.accepted());
    addManagedCounter(g_liveHitStatus.dropped, g_liveHitQueue.size());
    g_liveHitQueue.clear();
    addManagedCounter(g_liveHitStatus.dropped, g_liveIngress.clear());
    g_liveHitStatus.enabled = false;
    g_liveHitStatus.phase = "paused";
    return g_liveHitStatus;
}

void queueManagedLiveEvent(int projectileType, int damage, bool isKill, int npcIndex,
                           ManagedNpcSnapshot npc, ManagedPlayerSnapshot player) {
    if (!managedNpcSnapshotIsValid(npc) || isKill || npcIndex < 0 || npcIndex >= 200)
        npc = {};
    if (!managedPlayerSnapshotIsValid(player) || !npc.valid)
        player = {};
    const auto admission = g_liveIngress.admission();
    if (npcIndex < -1 || npcIndex >= 200 || (isKill && npcIndex != -1) ||
        damage < 0 || damage > 100000 || projectileType < 0 ||
        static_cast<size_t>(projectileType) >= g_additionalLiveByType.size()) return;
    const bool original = g_liveHitType.load(std::memory_order_acquire) == projectileType;
    const int bindingSlot = original ? -1 :
        g_additionalLiveByType[static_cast<size_t>(projectileType)].load(
            std::memory_order_acquire) - 1;
    if (!original && (bindingSlot < 0 ||
        bindingSlot >= g_additionalLiveCount.load(std::memory_order_acquire)))
        return;
    const uint64_t profiledAtNs = g_perfState.load(std::memory_order_relaxed) == 2
        ? static_cast<uint64_t>(perfNowNs()) : 0;
    ManagedLiveIngressPush result;
    if (original) {
        result = g_liveIngress.push(admission,
            {0, damage, isKill, profiledAtNs, -1, npcIndex, npc, player},
            projectileType, g_liveHitType, g_liveHitAutomatic);
    } else {
        AdditionalLiveBinding& binding =
            g_additionalLiveBindings[static_cast<size_t>(bindingSlot)];
        result = g_liveIngress.pushAdditional(admission,
            {0, damage, isKill, profiledAtNs, bindingSlot, npcIndex, npc, player},
            projectileType, binding.armedType);
        if (result.accepted)
            addManagedCounter(binding.queued);
    }
    if (result.queueDropped) {
        if (profiledAtNs)
            g_perfQueueDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!result.accepted) return;
    if (profiledAtNs) {
        g_perfAccepted.fetch_add(1, std::memory_order_relaxed);
        updatePerfMax(g_perfMaxQueueDepth, result.depth);
    }
    g_pinnedWake.notify_one();
}

void queueManagedLiveHit(int projectileType, int damage, int npcIndex,
                         ManagedNpcSnapshot npc, ManagedPlayerSnapshot player) {
    queueManagedLiveEvent(projectileType, damage, false, npcIndex, npc, player);
}

void queueManagedLiveKill(int projectileType, int damage) {
    queueManagedLiveEvent(projectileType, damage, true, -1, {}, {});
}

bool hasPendingManagedNpcBuffEffect() {
    return g_buffHookReady.load(std::memory_order_acquire) &&
        g_buffEffectQueue.depth() > 0;
}

void setManagedNpcBuffGameHookReady(bool ready) {
    g_buffHookReady.store(ready, std::memory_order_release);
}

ManagedNpcBuffEffectStatus managedNpcBuffEffectStatus() {
    ManagedNpcBuffEffectStatus status;
    status.gameHookInstalled = g_buffHookReady.load(std::memory_order_acquire);
    status.requested = g_buffRequested.load(std::memory_order_relaxed);
    status.applied = g_buffApplied.load(std::memory_order_relaxed);
    status.rejected = g_buffRejected.load(std::memory_order_relaxed);
    status.stale = g_buffStale.load(std::memory_order_relaxed);
    status.queueDrops = g_buffQueueDrops.load(std::memory_order_relaxed);
    status.queueDepth = g_buffEffectQueue.depth();
    status.lastBuffId = g_buffLastId.load(std::memory_order_relaxed);
    status.lastDurationTicks = g_buffLastDuration.load(std::memory_order_relaxed);
    status.lastNpcIndex = g_buffLastNpc.load(std::memory_order_relaxed);
    status.impulsesApplied = g_impulsesApplied.load(std::memory_order_relaxed);
    status.lastImpulseDirection = g_lastImpulseDirection.load(std::memory_order_relaxed);
    status.lastImpulseTenths = g_lastImpulseTenths.load(std::memory_order_relaxed);
    return status;
}

bool takeManagedNpcBuffForNpc(int npcIndex, int npcType,
                              float npcX, float npcY,
                              ManagedNpcBuffCommand* result) {
    if (!result || npcIndex < 0 || npcIndex >= 200 || npcType < 0 ||
        !std::isfinite(npcX) || !std::isfinite(npcY) ||
        !g_buffHookReady.load(std::memory_order_acquire)) return false;
    ManagedNpcBuffCommand request;
    for (int i = 0; i < 8 && g_buffEffectQueue.peek(&request); ++i) {
        const auto now = std::chrono::steady_clock::now();
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        if (request.generation != g_buffEffectGeneration.load(std::memory_order_acquire) ||
            !managedNpcBuffOwnerCurrent(request, g_buffOwnerGenerations) ||
            nowMs < request.createdAtMs || nowMs - request.createdAtMs > 1500) {
            g_buffEffectQueue.pop();
            g_buffStale.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (request.npcIndex != npcIndex) return false;
        g_buffEffectQueue.pop();
        if (request.npcType != npcType ||
            std::fabs(request.npcX - npcX) > 128.0f ||
            std::fabs(request.npcY - npcY) > 128.0f) {
            g_buffRejected.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        *result = request;
        g_buffLastId.store(request.buffId, std::memory_order_relaxed);
        g_buffLastDuration.store(request.durationTicks, std::memory_order_relaxed);
        g_buffLastNpc.store(request.npcIndex, std::memory_order_relaxed);
        g_lastImpulseDirection.store(request.impulseDirection, std::memory_order_relaxed);
        g_lastImpulseTenths.store(request.impulseTenths, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void noteManagedNpcBuffApplied(bool success) {
    if (success) g_buffApplied.fetch_add(1, std::memory_order_relaxed);
    else g_buffRejected.fetch_add(1, std::memory_order_relaxed);
}

void noteManagedNpcImpulseApplied(bool success) {
    noteManagedNpcBuffApplied(success);
    if (success) g_impulsesApplied.fetch_add(1, std::memory_order_relaxed);
}

bool isManagedLiveProjectileBound(int projectileType) {
    if (projectileType < 0 ||
        static_cast<size_t>(projectileType) >= g_additionalLiveByType.size())
        return false;
    const int slot = g_additionalLiveByType[static_cast<size_t>(projectileType)].load(
        std::memory_order_acquire) - 1;
    return slot >= 0 && slot < g_additionalLiveCount.load(std::memory_order_acquire) &&
        g_additionalLiveBindings[static_cast<size_t>(slot)].armedType.load(
            std::memory_order_acquire) == projectileType;
}

bool bindManagedLiveAdditional(const std::string& modId,
                               const std::string& assemblyPath,
                               int projectileType,
                               const std::string& projectile,
                               const std::string& callbackClass,
                               bool primaryForAdditionalMod,
                               std::string* error) {
    if (!error) return false;
    error->clear();
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        projectile.empty() || projectile.size() > 62 ||
        projectileType < 0 ||
        static_cast<size_t>(projectileType) >= g_additionalLiveByType.size() ||
        projectile.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos ||
        callbackClass.empty() || callbackClass.size() > 72 ||
        !validAssembly(assemblyPath)) {
        *error = "Invalid C# mod/projectile/assembly identity";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(modId)) {
        *error = "Live Mono callbacks were revoked; restart Bunny Loader";
        return false;
    }
    if (!g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveBenchmark.running || g_liveBenchmarkAttempted ||
        (g_liveHitStatus.eventLimit > 0 && g_liveHitStatus.queued >= g_liveHitStatus.eventLimit)) {
        *error = "Managed live event session unavailable";
        return false;
    }
    if (primaryForAdditionalMod) {
        if (modId == g_liveHitStatus.modId) {
            *error = "Second mod identity must differ from primary mod";
            return false;
        }
        for (int i = 0, count = g_additionalLiveCount.load(
                 std::memory_order_acquire); i < count; ++i) {
            const auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
            if (entry.modId == modId && entry.callbackClass == "BunnyLiveEvents") {
                *error = "Mod already has its primary live callbacks bound";
                return false;
            }
        }
    } else {
        bool belongs = modId == g_liveHitStatus.modId &&
                       assemblyPath == g_liveHitAssembly;
        for (int i = 0, count = g_additionalLiveCount.load(
                 std::memory_order_acquire); !belongs && i < count; ++i) {
            const auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
            belongs = entry.modId == modId && entry.assemblyPath == assemblyPath &&
                      entry.callbackClass == "BunnyLiveEvents" &&
                      entry.armedType.load(std::memory_order_acquire) == entry.type;
        }
        if (!belongs) {
            *error = "Secondary projectile requires primary callbacks for its own mod";
            return false;
        }
    }
    if (g_liveHitStatus.projectileType == projectileType ||
        g_additionalLiveByType[static_cast<size_t>(projectileType)].load(
            std::memory_order_acquire) != 0) {
        *error = "Projectile type already bound";
        return false;
    }
    const int count = g_additionalLiveCount.load(std::memory_order_relaxed);
    if (count >= static_cast<int>(g_additionalLiveBindings.size())) {
        *error = "Maximum of 31 additional bindings per session reached";
        return false;
    }
    auto& entry = g_additionalLiveBindings[static_cast<size_t>(count)];
    entry.modId = modId;
    entry.projectile = projectile;
    entry.callbackClass = callbackClass;
    entry.assemblyPath = assemblyPath;
    entry.type = projectileType;
    entry.processed = entry.hits = entry.kills = entry.failures = 0;
    entry.dropped = entry.lastResult = 0;
    entry.hitContext = {};
    entry.error.clear();
    entry.queued.store(0, std::memory_order_relaxed);
    entry.armedType.store(projectileType, std::memory_order_release);
    g_additionalLiveCount.store(count + 1, std::memory_order_release);
    g_additionalLiveByType[static_cast<size_t>(projectileType)].store(
        static_cast<int16_t>(count + 1), std::memory_order_release);
    return true;
}

bool enableManagedLiveMod(const std::string& modId,
                          const std::string& assemblyPath,
                          int projectileType,
                          const std::string& projectile,
                          std::string* error) {
    return bindManagedLiveAdditional(modId, assemblyPath, projectileType,
                                     projectile, "BunnyLiveEvents", true, error);
}

bool enableManagedLiveProjectile(const std::string& modId,
                                const std::string& assemblyPath,
                                int projectileType,
                                const std::string& projectile,
                                std::string* error) {
    return bindManagedLiveAdditional(modId, assemblyPath, projectileType,
                                     projectile, projectile + "LiveEvents", false, error);
}

bool pauseManagedLiveMod(const std::string& modId, std::string* error) {
    if (!error) return false;
    error->clear();
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (modId.empty() || modId == g_liveHitStatus.modId) {
        *error = "Only an additional C# mod can be paused independently";
        return false;
    }
    const int count = g_additionalLiveCount.load(std::memory_order_acquire);
    if (g_liveHitInFlight &&
        g_liveHitActiveBindingSlot >= 0 &&
        g_liveHitActiveBindingSlot < count &&
        g_additionalLiveBindings[
            static_cast<size_t>(g_liveHitActiveBindingSlot)].modId == modId) {
        *error = "This C# mod has a callback in flight; retry after it completes";
        return false;
    }
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
        if (entry.modId != modId) continue;
        if (entry.armedType.load(std::memory_order_acquire) != entry.type)
            continue;
        changed = true;
        const int discarded = g_liveIngress.pauseBinding(i, entry.armedType);
        addManagedCounter(entry.dropped, discarded);
        addManagedCounter(g_liveHitStatus.dropped, discarded);
    }
    if (!changed) *error = "Mod not armed or already paused";
    return changed;
}

bool resumeManagedLiveMod(const std::string& modId, std::string* error) {
    if (!error) return false;
    error->clear();
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos) {
        *error = "Invalid C# mod identity";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_revokedDeferredMods.contains(modId)) {
        *error = "Live Mono callbacks were revoked; restart Bunny Loader";
        return false;
    }
    if (!g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveHitStatus.phase == "draining" ||
        g_liveBenchmark.running || g_liveBenchmarkAttempted ||
        (g_liveHitStatus.eventLimit > 0 && g_liveIngress.accepted() >= g_liveHitStatus.eventLimit)) {
        *error = "Live C# session unavailable; re-enter the world";
        return false;
    }
    if (modId == g_liveHitStatus.modId) {
        *error = "Primary mod cannot be resumed independently";
        return false;
    }
    const int count = g_additionalLiveCount.load(std::memory_order_acquire);
    bool hasPrimary = false;
    bool hasPaused = false;
    for (int i = 0; i < count; ++i) {
        const auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
        if (entry.modId != modId) continue;
        if (entry.callbackClass == "BunnyLiveEvents") hasPrimary = true;
        if (entry.type < 0 ||
            static_cast<size_t>(entry.type) >= g_additionalLiveByType.size() ||
            g_additionalLiveByType[static_cast<size_t>(entry.type)].load(
                std::memory_order_acquire) != i + 1 ||
            entry.armedType.load(std::memory_order_acquire) == entry.type ||
            entry.failures >= 3 || !entry.error.empty() ||
            !validAssembly(entry.assemblyPath)) {
            *error = "Mod bindings are not safely paused; cannot resume";
            return false;
        }
        hasPaused = true;
    }
    if (!hasPrimary || !hasPaused) {
        *error = "No paused secondary C# mod found";
        return false;
    }
    for (int i = 0; i < count; ++i) {
        auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
        if (entry.modId != modId) continue;
        entry.armedType.store(entry.type, std::memory_order_release);
    }
    return true;
}

std::vector<ManagedLiveProjectileStatus> managedLiveProjectileStatuses() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    std::vector<ManagedLiveProjectileStatus> results;
    const int count = g_additionalLiveCount.load(std::memory_order_acquire);
    results.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto& entry = g_additionalLiveBindings[static_cast<size_t>(i)];
        ManagedLiveProjectileStatus status;
        status.modId = entry.modId;
        status.projectile = entry.projectile;
        status.callbackClass = entry.callbackClass;
        status.projectileType = entry.type;
        status.enabled = entry.armedType.load(std::memory_order_acquire) == entry.type;
        status.queued = entry.queued.load(std::memory_order_acquire);
        status.processed = entry.processed;
        status.hits = entry.hits;
        status.kills = entry.kills;
        status.failures = entry.failures;
        status.dropped = entry.dropped;
        addManagedCounter(status.dropped, g_liveIngress.bindingDrops(i));
        status.lastResult = entry.lastResult;
        status.hitContext = entry.hitContext;
        status.error = entry.error;
        results.push_back(std::move(status));
    }
    return results;
}

bool startManagedLiveBenchmark(std::string* error) {
    if (!error) return false;
    error->clear();
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (g_liveBenchmarkAttempted) {
        *error = "Benchmark ja realizado: reinicie Bunny Loader para repetir";
        return false;
    }
    if (!g_pinnedStarted || g_pinnedBusy || g_pinnedPending ||
        !g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveHitStatus.modId != kBenchmarkModId ||
        g_liveHitStatus.projectileType < 0 || g_liveHitStatus.failures ||
        g_liveHitInFlight || !g_liveHitQueue.empty() ||
        g_liveIngress.depth() != 0) {
        *error = "Aguarde os callbacks automaticos e fila vazia em singleplayer";
        return false;
    }

    const ManagedLiveHitStatus previous = g_liveHitStatus;
    g_liveHitType.store(-1, std::memory_order_release);
    disarmAdditionalLiveBindings();
    g_liveHitAutomatic.store(false, std::memory_order_release);
    g_liveIngress.reset();
    ++g_liveHitGeneration;
    g_liveHitStatus = {};
    g_primaryLiveFailures = 0;
    g_liveHitStatus.enabled = true;
    g_liveHitStatus.automatic = true;
    g_liveHitStatus.modId = previous.modId;
    g_liveHitStatus.projectileType = previous.projectileType;
    g_liveHitStatus.phase = "benchmark-armed";
    g_liveHitLockDrops.store(0, std::memory_order_relaxed);
    g_liveBenchmark = {};
    g_liveBenchmark.started = true;
    g_liveBenchmark.running = true;
    g_liveBenchmark.phase = "running";
    g_liveBenchmarkStart = std::chrono::steady_clock::now();
    pthread_t thread{};
    const int created = pthread_create(&thread, nullptr, &managedBenchmarkProducer, nullptr);
    if (created != 0) {
        g_liveHitStatus = previous;
        g_liveHitAutomatic.store(previous.automatic, std::memory_order_release);
        g_liveHitType.store(previous.projectileType, std::memory_order_release);
        g_liveBenchmark = {};
        *error = "pthread_create falhou: " + std::to_string(created);
        return false;
    }
    g_liveBenchmarkAttempted = true;
    pthread_detach(thread);
    return true;
}

ManagedLiveBenchmarkStatus managedLiveBenchmarkStatus() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    ManagedLiveBenchmarkStatus status = g_liveBenchmark;
    if (!status.started) return status;
    status.accepted = g_liveHitStatus.queued;
    status.processed = g_liveHitStatus.processed;
    status.hits = g_liveHitStatus.hitProcessed;
    status.kills = g_liveHitStatus.killProcessed;
    status.maxQueueDepth = g_liveHitStatus.maxQueueDepth;
    status.dropped = g_liveHitStatus.dropped +
        g_liveHitLockDrops.load(std::memory_order_relaxed) +
        g_liveIngress.dropped();
    const auto end = status.running ?
        std::chrono::steady_clock::now() : g_liveBenchmarkEnd;
    status.elapsedMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end - g_liveBenchmarkStart).count());
    return status;
}

ManagedLiveBenchmarkStatus stopManagedLiveBenchmark() {
    {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        if (g_liveBenchmark.running)
            finishBenchmarkErrorLocked("benchmark-cancelled", "Cancelado pelo usuario");
    }
    return managedLiveBenchmarkStatus();
}

bool beginVmCompare(const VmCompareStatus& jsPart, std::string* error) {
    if (!error) return false;
    error->clear();
    if (jsPart.kind < 0 || jsPart.kind > 2 ||
        (jsPart.kind == 0 && (jsPart.iterations < 1000 || jsPart.iterations > 5000000)) ||
        (jsPart.kind != 0 && (jsPart.iterations < 64 || jsPart.iterations > 5000))) {
        *error = "Invalid benchmark mode or iteration count";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    if (!g_pinnedStarted || g_pinnedBusy || g_pinnedPending ||
        g_vmComparePending || g_vmCompare.running ||
        g_vmCompareRuns >= 4 ||
        g_liveBenchmark.running || g_liveHitInFlight ||
        !g_liveHitQueue.empty() || g_liveIngress.depth() != 0 ||
        !g_liveHitStatus.automatic || !g_liveHitStatus.enabled ||
        g_liveHitStatus.modId != "68c3115f-62f4-49f2-a56b-388256781d5f") {
        *error = "MonoVM not idle, ExampleModCSharp inactive, or 4-run limit reached";
        return false;
    }
    g_vmCompare = jsPart;
    g_vmCompare.started = true;
    g_vmCompare.running = true;
    g_vmCompare.phase = "mono-running";
    g_vmComparePending = true;
    ++g_vmCompareRuns;
    g_pinnedWake.notify_one();
    return true;
}

VmCompareStatus vmCompareStatus() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    return g_vmCompare;
}

bool startManagedLivePerf(std::string* error) {
    if (!error) return false;
    error->clear();
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    const int state = g_perfState.load(std::memory_order_acquire);
    if (state == 1 || state == 2) {
        *error = "Monitor de desempenho ja esta ativo";
        return false;
    }
    if (!g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveBenchmark.running || g_vmComparePending || g_vmCompare.running ||
        g_liveHitStatus.failures != 0 ||
        g_liveHitStatus.modId != kBenchmarkModId) {
        *error = "Aguarde o ExampleModCSharp e os callbacks automaticos em singleplayer";
        return false;
    }
    resetPerfCounters();
    g_perfLockDropBaseline.store(
        g_liveHitLockDrops.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    g_perfState.store(1, std::memory_order_release);
    return true;
}

void noteManagedLivePerfUpdate() {
    int state = g_perfState.load(std::memory_order_relaxed);
    if (state != 1 && state != 2) return;
    const int64_t now = perfNowNs();
    if (state == 1) {
        int expected = 1;
        if (g_perfState.compare_exchange_strong(
                expected, 2, std::memory_order_acq_rel)) {
            g_perfStartNs.store(now, std::memory_order_release);
            g_perfPreviousUpdateNs.store(now, std::memory_order_release);
        }
        return;
    }
    const int64_t started = g_perfStartNs.load(std::memory_order_acquire);
    if (started <= 0) return;
    if (now - started >= kPerfDurationNs) {
        int expected = 2;
        if (g_perfState.compare_exchange_strong(
                expected, 3, std::memory_order_acq_rel))
            g_perfEndNs.store(now, std::memory_order_release);
        return;
    }
    const int64_t before = g_perfPreviousUpdateNs.exchange(
        now, std::memory_order_relaxed);
    if (before <= 0 || now <= before) return;
    const int64_t intervalUs = (now - before) / 1000;
    if (intervalUs <= 0) return;
    const int index = static_cast<int>(
        std::min<int64_t>(intervalUs / 4000, 25));
    g_perfUpdateBins[index].fetch_add(1, std::memory_order_relaxed);
    g_perfUpdateSamples.fetch_add(1, std::memory_order_relaxed);
    g_perfUpdateTotalUs.fetch_add(intervalUs, std::memory_order_relaxed);
    if (intervalUs > 25000)
        g_perfOver25.fetch_add(1, std::memory_order_relaxed);
    if (intervalUs > 33000)
        g_perfOver33.fetch_add(1, std::memory_order_relaxed);
}

void noteManagedLivePerfProjectileCurveUpdate() {
    if (g_perfState.load(std::memory_order_relaxed) == 2)
        g_perfProjectileCurveUpdates.fetch_add(1, std::memory_order_relaxed);
}

void noteManagedLivePerfProjectileVelocityWrite() {
    if (g_perfState.load(std::memory_order_relaxed) == 2)
        g_perfProjectileVelocityWrites.fetch_add(1, std::memory_order_relaxed);
}

ManagedLivePerfStatus managedLivePerfStatus() {
    ManagedLivePerfStatus result;
    const int state = g_perfState.load(std::memory_order_acquire);
    result.started = state != 0;
    result.recording = state == 2;
    result.phase = state == 0 ? "idle" : state == 1 ? "armed" :
        state == 2 ? "recording" : state == 3 ? "completed" : "stopped";
    const int64_t start = g_perfStartNs.load(std::memory_order_acquire);
    const int64_t end = state == 2 ? perfNowNs() :
        g_perfEndNs.load(std::memory_order_acquire);
    if (start > 0 && end >= start)
        result.elapsedMs = static_cast<int>(
            std::min<int64_t>((end - start) / 1000000, 60000));
    result.updateSamples = g_perfUpdateSamples.load(std::memory_order_relaxed);
    const int64_t intervalTotal =
        g_perfUpdateTotalUs.load(std::memory_order_relaxed);
    if (result.updateSamples > 0 && intervalTotal > 0)
        result.updateHz = static_cast<double>(result.updateSamples) * 1000000.0 /
            static_cast<double>(intervalTotal);
    result.updatesOver25Ms = g_perfOver25.load(std::memory_order_relaxed);
    result.updatesOver33Ms = g_perfOver33.load(std::memory_order_relaxed);
    if (result.updateSamples > 0) {
        const int target = (result.updateSamples * 9 + 9) / 10;
        int count = 0;
        for (int i = 0; i < 26; ++i) {
            count += static_cast<int>(
                g_perfUpdateBins[i].load(std::memory_order_relaxed));
            if (count >= target) {
                result.updateP90Ms = static_cast<double>((i + 1) * 4);
                break;
            }
        }
        const int target99 = (result.updateSamples * 99 + 99) / 100;
        count = 0;
        for (int i = 0; i < 26; ++i) {
            count += static_cast<int>(
                g_perfUpdateBins[i].load(std::memory_order_relaxed));
            if (count >= target99) {
                result.updateP99Ms = static_cast<double>((i + 1) * 4);
                break;
            }
        }
    }
    result.callbacksSampled =
        g_perfCallbacks.load(std::memory_order_relaxed);
    if (result.callbacksSampled > 0) {
        result.queueWaitAvgUs = static_cast<double>(
            g_perfQueueTotalUs.load(std::memory_order_relaxed)) /
            result.callbacksSampled;
        result.callbackAvgUs = static_cast<double>(
            g_perfCallbackTotalUs.load(std::memory_order_relaxed)) /
            result.callbacksSampled;
        result.callbackCpuAvgUs = static_cast<double>(
            g_perfCallbackCpuTotalUs.load(std::memory_order_relaxed)) /
            result.callbacksSampled;
        result.callbackP90Us = perfPercentileUs(
            g_perfCallbackBins, result.callbacksSampled, 90);
        result.callbackP99Us = perfPercentileUs(
            g_perfCallbackBins, result.callbacksSampled, 99);
        result.callbackCpuP90Us = perfPercentileUs(
            g_perfCallbackCpuBins, result.callbacksSampled, 90);
    }
    result.queueWaitSamples = g_perfQueueSamples.load(std::memory_order_relaxed);
    if (result.queueWaitSamples > 0) {
        result.queueWaitAvgUs = static_cast<double>(
            g_perfQueueTotalUs.load(std::memory_order_relaxed)) /
            result.queueWaitSamples;
        result.queueWaitP90Us = perfPercentileUs(
            g_perfQueueBins, result.queueWaitSamples, 90);
        result.queueWaitP99Us = perfPercentileUs(
            g_perfQueueBins, result.queueWaitSamples, 99);
    }
    result.queueWaitMaxUs = g_perfQueueMaxUs.load(std::memory_order_relaxed);
    result.callbackMaxUs = g_perfCallbackMaxUs.load(std::memory_order_relaxed);
    result.callbackCpuMaxUs = g_perfCallbackCpuMaxUs.load(std::memory_order_relaxed);
    result.eventsAccepted = g_perfAccepted.load(std::memory_order_relaxed);
    result.eventsProcessed = g_perfProcessed.load(std::memory_order_relaxed);
    result.eventsFailed = g_perfFailed.load(std::memory_order_relaxed);
    result.eventsQueueDropped = g_perfQueueDropped.load(std::memory_order_relaxed);
    result.maxQueueDepth = g_perfMaxQueueDepth.load(std::memory_order_relaxed);
    result.projectileCurveUpdates =
        g_perfProjectileCurveUpdates.load(std::memory_order_relaxed);
    result.projectileVelocityWrites =
        g_perfProjectileVelocityWrites.load(std::memory_order_relaxed);
    const int currentDrops =
        g_liveHitLockDrops.load(std::memory_order_relaxed);
    const int baseline =
        g_perfLockDropBaseline.load(std::memory_order_relaxed);
    result.lockDrops = result.started ? std::max(0, currentDrops - baseline) : 0;
    if (result.started) {
        std::lock_guard<std::mutex> lock(g_pinnedMutex);
        auto append = [&](int slot, const std::string& modId,
                          const std::string& callbackClass, int type, bool enabled) {
            if (type < 0) return;
            const auto& counters = g_perfProjectileCounters[static_cast<size_t>(slot)];
            ManagedLivePerfProjectileStatus item;
            item.modId = modId;
            item.callbackClass = callbackClass;
            item.projectileType = type;
            item.enabled = enabled;
            item.callbacksSampled = counters.callbacks.load(std::memory_order_relaxed);
            item.hitSamples = counters.hits.load(std::memory_order_relaxed);
            item.killSamples = counters.kills.load(std::memory_order_relaxed);
            item.failures = counters.failures.load(std::memory_order_relaxed);
            item.queueWaitSamples = counters.queueSamples.load(std::memory_order_relaxed);
            const int64_t queueUs = counters.queueTotalUs.load(std::memory_order_relaxed);
            if (item.queueWaitSamples > 0)
                item.queueWaitAvgUs = static_cast<double>(queueUs) / item.queueWaitSamples;
            item.queueWaitMaxUs = counters.queueMaxUs.load(std::memory_order_relaxed);
            const int64_t callbackUs = counters.callbackTotalUs.load(std::memory_order_relaxed);
            const int64_t cpuUs = counters.cpuTotalUs.load(std::memory_order_relaxed);
            if (item.callbacksSampled > 0) {
                item.callbackAvgUs = static_cast<double>(callbackUs) / item.callbacksSampled;
                item.callbackCpuAvgUs = static_cast<double>(cpuUs) / item.callbacksSampled;
                item.callbackP90Us = perfPercentileUs(
                    counters.callbackBins, item.callbacksSampled, 90);
            }
            item.callbackMaxUs = counters.callbackMaxUs.load(std::memory_order_relaxed);
            item.callbackCpuMaxUs = counters.cpuMaxUs.load(std::memory_order_relaxed);
            result.projectiles.push_back(std::move(item));
        };
        append(0, g_liveHitStatus.modId, "BunnyLiveEvents",
               g_liveHitStatus.projectileType,
               g_liveHitType.load(std::memory_order_acquire) >= 0);
        const int count = g_additionalLiveCount.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i) {
            const auto& binding = g_additionalLiveBindings[static_cast<size_t>(i)];
            append(i + 1, binding.modId, binding.callbackClass, binding.type,
                   binding.armedType.load(std::memory_order_acquire) == binding.type);
        }
    }
    return result;
}

ManagedLivePerfStatus stopManagedLivePerf() {
    int expected = 2;
    if (g_perfState.compare_exchange_strong(
        expected, 4, std::memory_order_acq_rel))
        g_perfEndNs.store(perfNowNs(), std::memory_order_release);
    expected = 1;
    g_perfState.compare_exchange_strong(
        expected, 4, std::memory_order_acq_rel);
    return managedLivePerfStatus();
}

long managedRuntimePinnedThreadTid() {
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    return g_pinnedTid;
}

ManagedRuntimeStatus probeManagedCallbacks(const std::string& assemblyPath, int count) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.callbacksExecuted = 0;
    g_status.error.clear();
    if (!g_domain || !g_status.domainInitialized)
        return failure("callback-runtime-not-ready", "Executar managedGameProbe antes dos callbacks");
    if (!validAssembly(assemblyPath))
        return failure("callback-assembly-missing", "BunnyManagedProbe.dll ausente");
    if (count < 1 || count > 120)
        return failure("callback-count-invalid", "Quantidade deve ser entre 1 e 120");
    if (!g_api.attach(g_domain))
        return failure("callback-attach-failed", "mono_thread_attach retornou nulo");

    trace("callback-opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
    if (!assembly)
        return failure("callback-assembly-failed", "Nao foi possivel carregar BunnyManagedProbe");
    void* img = g_api.image(assembly);
    void* klass = img ? g_api.findClass(img, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "AI", 2) : nullptr;
    if (!method)
        return failure("callback-method-missing", "Probe.AI(int,int) nao encontrado");

    int state = 0;
    int expected = 0;
    trace("invoking-ai-callbacks");
    for (int tick = 0; tick < count; ++tick) {
        int tickArg = tick;
        int stateArg = state;
        void* args[] = {&tickArg, &stateArg};
        void* exception = nullptr;
        void* boxed = g_api.invoke(method, nullptr, args, &exception);
        if (exception || !boxed)
            return failure("callback-invoke-failed", "Probe.AI lancou excecao ou retornou nulo");
        void* value = g_api.unbox(boxed);
        if (!value)
            return failure("callback-unbox-failed", "Probe.AI retornou valor invalido");
        std::memcpy(&state, value, sizeof(state));
        expected += (tick % 7) - 3;
        ++g_status.callbacksExecuted;
        if (state != expected)
            return failure("callback-mismatch", "Probe.AI retornou resultado incorreto");
    }
    g_status.result = state;
    g_status.methodExecuted = true;
    trace("callback-verified");
    return g_status;
}

ManagedSourceCompileResult compileManagedSource(
    const std::string& probeAssembly,
    const std::string& compilerAssembly,
    const std::string& modRoot,
    const std::string& outputAssembly,
    const std::string& diagnosticsPath,
    bool precompileOnly,
    bool verifyOnly) {
    ManagedSourceCompileResult out;
    if (modRoot.empty() || outputAssembly.empty() || diagnosticsPath.empty() ||
        modRoot.size() > 2048 || outputAssembly.size() > 2048 ||
        diagnosticsPath.size() > 2048 ||
        modRoot.find("..") != std::string::npos ||
        outputAssembly.find("..") != std::string::npos ||
        diagnosticsPath.find("..") != std::string::npos) {
        out.phase = "compile-path-invalid";
        out.error = "Diretorio do mod ou saida invalida";
        return out;
    }
    const auto probeStarted = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        if (!g_compilerAotAttempted) {
            g_compilerAotAttempted = true;
            const auto slash = compilerAssembly.find_last_of('/');
            if (slash == std::string::npos) g_compilerAot.reason = "FrameworkPathInvalid";
            else if (g_domain) g_compilerAot.reason = "RuntimeAlreadyInitialized";
            else if (loadLibrary()) {
                Dl_info runtime{};
                const auto registerModule = resolve<void (*)(void**)>("mono_aot_register_module");
                if (dladdr(reinterpret_cast<void*>(g_api.initialize), &runtime) && runtime.dli_fname)
                    g_compilerAot = installCompilerAot("libbunnycompileraot.so",
                        compilerAssembly.substr(0, slash), runtime.dli_fname, registerModule);
                else g_compilerAot.reason = "RuntimePathUnavailable";
            } else g_compilerAot.reason = "RuntimeLibraryUnavailable";
            g_compilerAotSetupMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - probeStarted).count();
        }
    }
    const auto initialized = probeManagedRuntime(probeAssembly);
    out.monoProbeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - probeStarted).count();
    if (!initialized.domainInitialized || !initialized.methodExecuted) {
        out.phase = "compile-mono-unavailable";
        out.error = initialized.error;
        return out;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    if (!validAssembly(compilerAssembly) || !g_api.newString) {
        out.phase = "compile-host-unavailable";
        out.error = "Roslyn assembly ou mono_string_new indisponivel";
        trace("compile-host-unavailable", out.error.c_str());
        return out;
    }
    trace("compile-opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, compilerAssembly.c_str());
    void* image = assembly ? g_api.image(assembly) : nullptr;
    void* klass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "Compiler") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "Compile", 4) : nullptr;
    if (!method) {
        out.phase = "compile-method-missing";
        out.error = "Compiler.Compile(string,string,string,string) indisponivel";
        trace("compile-method-missing", out.error.c_str());
        return out;
    }
    if (g_compilerAot.registered && g_compilerMethodCode == "NotChecked") {
        const auto compileMethod = resolve<void* (*)(void*)>("mono_compile_method");
        void* code = compileMethod ? compileMethod(method) : nullptr;
        Dl_info origin{};
        g_compilerMethodCode = code && dladdr(code, &origin) && origin.dli_fname &&
            std::strstr(origin.dli_fname, "libbunnycompileraot.so") ? "AOT" : "JITOrUnavailable";
    }
    const std::string aotStatus = "pid=" + std::to_string(getpid()) +
        "\nrequestedMode=MixedJitAot\nregistered=" + std::to_string(g_compilerAot.registered) +
        "\nreason=" + g_compilerAot.reason + "\nsetupMs=" + std::to_string(g_compilerAotSetupMs) +
        "\ndetail=" + g_compilerAot.detail +
        "\ncompilerMethodCode=" + g_compilerMethodCode +
        "\nmodSources=Editable\nfiveSecondTarget=Unmeasured\n";
    const auto diagnosticSlash = diagnosticsPath.find_last_of('/');
    if (diagnosticSlash != std::string::npos) {
        const std::string path = diagnosticsPath.substr(0, diagnosticSlash + 1) + "source-compiler-aot-status.txt";
        if (FILE* file = std::fopen(path.c_str(), "wb")) {
            std::fwrite(aotStatus.data(), 1, aotStatus.size(), file);
            std::fclose(file);
        }
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
        "[CSharp][CompilerAot] Registered=%d Reason=%s SetupMs=%lld CompilerMethodCode=%s Detail=%s",
        g_compilerAot.registered, g_compilerAot.reason.c_str(), g_compilerAotSetupMs,
        g_compilerMethodCode.c_str(), g_compilerAot.detail.c_str());
#endif
    void* preflightClass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "Preflight") : nullptr;
    void* ping = preflightClass ? g_api.findMethod(preflightClass, "Ping", 1) : nullptr;
    if (!ping) {
        out.phase = "compile-preflight-missing";
        out.error = "Preflight.Ping(string) indisponivel";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    trace("compile-preflight-start");
    void* pingArgument = g_api.newString(g_domain, diagnosticsPath.c_str());
    if (!pingArgument) {
        out.phase = "compile-preflight-argument-failed";
        trace(out.phase.c_str());
        return out;
    }
    void* pingArgs[] = { pingArgument };
    void* pingException = nullptr;
    void* pingResult = g_api.invoke(ping, nullptr, pingArgs, &pingException);
    if (pingException || !pingResult || !g_api.unbox(pingResult)) {
        out.phase = "compile-preflight-exception";
        out.error = "Falha antes de carregar a rotina Roslyn";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    int pingValue = 0;
    std::memcpy(&pingValue, g_api.unbox(pingResult), sizeof(pingValue));
    if (pingValue != 42) {
        out.phase = "compile-preflight-invalid-result";
        trace(out.phase.c_str());
        return out;
    }
    trace("compile-preflight-verified");

    if (!verifyOnly) {
    const auto slash = compilerAssembly.find_last_of('/');
    if (slash == std::string::npos) {
        out.phase = "compile-framework-invalid";
        return out;
    }
    const std::string frameworkDir = compilerAssembly.substr(0, slash);
    void* argsManaged[] = {
        g_api.newString(g_domain, modRoot.c_str()),
        g_api.newString(g_domain, outputAssembly.c_str()),
        g_api.newString(g_domain, frameworkDir.c_str()),
        g_api.newString(g_domain, diagnosticsPath.c_str())
    };
    if (!argsManaged[0] || !argsManaged[1] || !argsManaged[2] || !argsManaged[3]) {
        out.phase = "compile-arguments-failed";
        return out;
    }
    void* args[] = {
        argsManaged[0], argsManaged[1], argsManaged[2], argsManaged[3]
    };
    void* exception = nullptr;
    rearmNativeAbortProbe("after-mono-preflight-before-roslyn");
    static bool compilerSignalTraceInstalled = false;
    if (!compilerSignalTraceInstalled) {
        installDestroyedMutexTrace();
        installNativeSegvTrace();
        compilerSignalTraceInstalled = true;
    }
    trace("compiling-source");
    const auto roslynStarted = std::chrono::steady_clock::now();
    void* boxed = g_api.invoke(method, nullptr, args, &exception);
    out.roslynInvokeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - roslynStarted).count();
    if (exception || !boxed) {
        out.phase = "compile-managed-exception";
        out.error = "Excecao no compilador gerenciado, consultar logcat";
        trace("compile-managed-exception", out.error.c_str());
        return out;
    }
    void* raw = g_api.unbox(boxed);
    if (!raw) {
        out.phase = "compile-return-invalid";
        trace("compile-return-invalid");
        return out;
    }
    std::memcpy(&out.result, raw, sizeof(out.result));
    if (out.result <= 0) {
        out.phase = "compile-failed";
        trace(out.phase.c_str());
        return out;
    }

    if (precompileOnly) {
        out.phase = "compile-prefetched";
        trace(out.phase.c_str());
        return out;
    }
    } else {
        out.result = 2;
        trace("compile-prefetch-reuse-validating");
    }
    trace("compiled-assembly-execution-start");
    void* verifierClass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "CompiledAssemblyProbe") : nullptr;
    void* verifier = verifierClass ? g_api.findMethod(verifierClass, "Verify", 3) : nullptr;
    if (!verifier) {
        out.phase = "compiled-assembly-verifier-missing";
        out.error = "CompiledAssemblyProbe.Verify indisponivel";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    void* verifyArgumentsManaged[] = {
        g_api.newString(g_domain, outputAssembly.c_str()),
        g_api.newString(g_domain, diagnosticsPath.c_str()),
        g_api.newString(g_domain, modRoot.c_str())
    };
    if (!verifyArgumentsManaged[0] || !verifyArgumentsManaged[1] || !verifyArgumentsManaged[2]) {
        out.phase = "compiled-assembly-verifier-arguments-failed";
        trace(out.phase.c_str());
        return out;
    }
    void* verifyArguments[] = { verifyArgumentsManaged[0], verifyArgumentsManaged[1], verifyArgumentsManaged[2] };
    void* verifyException = nullptr;
    const auto verifyStarted = std::chrono::steady_clock::now();
    void* verifiedBoxed = g_api.invoke(verifier, nullptr, verifyArguments, &verifyException);
    out.verifyInvokeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - verifyStarted).count();
    if (verifyException || !verifiedBoxed || !g_api.unbox(verifiedBoxed)) {
        out.phase = "compiled-assembly-verifier-exception";
        out.error = "Falha ao executar assembly C# compilado";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    std::memcpy(&out.executedTypes, g_api.unbox(verifiedBoxed), sizeof(out.executedTypes));
    if (!validSourceExecutedTypeCount(out.executedTypes)) {
        out.phase = "compiled-assembly-execution-failed";
        out.error = "[CSharp][ERROR][CompileVerify] Method=CompiledAssemblyProbe.Verify Stage=ManagedVerify Expected=1..8192 Actual="+
            std::to_string(out.executedTypes)+" Result=ModRejected DiagnosticsPath="+diagnosticsPath;
        if(FILE* diagnostics=std::fopen(diagnosticsPath.c_str(),"rb")) {
            std::fseek(diagnostics,0,SEEK_END);const long size=std::ftell(diagnostics);
            if(size>0) {
                const long start=size>8192?size-8192:0;std::fseek(diagnostics,start,SEEK_SET);
                char detail[8193]{};const size_t read=std::fread(detail,1,8192,diagnostics);
                out.error+=" Detail="+std::string(detail,read);
            }
            std::fclose(diagnostics);
        }
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    if (void* moduleGetter = g_api.findMethod(verifierClass, "get_LastVerifiedHookModules", 0)) {
        void* moduleException = nullptr;
        void* modulesBoxed = g_api.invoke(moduleGetter, nullptr, nullptr, &moduleException);
        if (moduleException || !modulesBoxed || !g_api.unbox(modulesBoxed)) {
            out.phase = "compiled-module-inventory-failed";
            out.error = "[CSharp][ERROR][CompileVerify] Stage=ReadHookModuleInventory Result=Rejected Reason=ManagedInventoryUnavailable";
            return out;
        }
        std::memcpy(&out.hookModules, g_api.unbox(modulesBoxed), sizeof(out.hookModules));
        if (out.hookModules < 0 || out.hookModules > 4096 || out.hookModules > out.executedTypes) {
            out.phase = "compiled-module-inventory-failed";
            out.error = "[CSharp][ERROR][CompileVerify] Stage=ReadHookModuleInventory Expected=0..4096WithinExecutedTypes Actual=" +
                std::to_string(out.hookModules) + " Result=Rejected Reason=ManagedInventoryOutOfBounds";
            return out;
        }
    }
    trace("compiled-assembly-execution-verified");
    out.phase = "compile-verified";
    trace(out.phase.c_str());
    return out;
}

void invalidateManagedDeferredSourceMod(const std::string& modId) {
    if (modId.empty()) return;
    std::lock_guard<std::mutex> lock(g_pinnedMutex);
    g_revokedDeferredMods.insert(modId);
    for (auto& binding : g_universalMods) {
        if (binding.id != modId) continue;
        binding.registered = true;
        if (std::find(g_universalPendingUnloads.begin(),
                      g_universalPendingUnloads.end(), modId) ==
            g_universalPendingUnloads.end())
            g_universalPendingUnloads.push_back(modId);
        g_universalPolling.store(true, std::memory_order_release);
        g_pinnedWake.notify_one();
        break;
    }

    const bool primaryRevoked = g_liveHitStatus.modId == modId &&
        g_liveHitStatus.enabled;
    if (g_liveHitStatus.modId == modId)
        g_buffOwnerGenerations[0].fetch_add(1, std::memory_order_release);
    if (primaryRevoked) {
        const int discarded = g_liveIngress.pauseBinding(-1, g_liveHitType);
        addManagedCounter(g_liveHitStatus.dropped, discarded);
        addManagedCounter(g_liveHitStatus.dropped, g_liveHitQueue.size());
        g_liveHitQueue.clear();
        g_liveHitStatus.phase = "primary-mod-revoked";
    }
    const int liveCount = g_additionalLiveCount.load(std::memory_order_acquire);
    bool anyAdditionalArmed = false;
    for (int i = 0; i < liveCount; ++i) {
        auto& binding = g_additionalLiveBindings[static_cast<size_t>(i)];
        if (binding.modId == modId) {
            g_buffOwnerGenerations[static_cast<size_t>(i + 1)].fetch_add(
                1, std::memory_order_release);
            const int discarded = g_liveIngress.pauseBinding(i, binding.armedType);
            addManagedCounter(binding.dropped, discarded);
            addManagedCounter(g_liveHitStatus.dropped, discarded);
            if (binding.type >= 0 &&
                static_cast<size_t>(binding.type) < g_additionalLiveByType.size() &&
                g_additionalLiveByType[static_cast<size_t>(binding.type)].load(
                    std::memory_order_acquire) == i + 1)
                g_additionalLiveByType[static_cast<size_t>(binding.type)].store(
                    0, std::memory_order_release);
        }
        if (binding.armedType.load(std::memory_order_acquire) == binding.type)
            anyAdditionalArmed = true;
    }
    if (managedLiveSessionCanStop(
            g_liveHitStatus.enabled,
            g_revokedDeferredMods.contains(g_liveHitStatus.modId),
            g_liveHitType.load(std::memory_order_acquire) < 0,
            anyAdditionalArmed)) {
        g_liveHitStatus.enabled = false;
        g_liveHitAutomatic.store(false, std::memory_order_release);
        g_liveHitStatus.phase = "mod-revoked";
    }

    for (auto& binding : g_sceneLightBindings)
        if (binding.modId == modId) binding.enabled = false;
    g_sceneLightResults.erase(modId);
    eraseManagedModBoundJobs(g_sceneLightQueue, g_sceneLightBindings, modId);

    for (auto& binding : g_managedHurtBindings)
        if (binding.modId == modId) binding.active = false;
    const auto cancelledHurt = std::erase_if(g_managedHurtQueue, [&](auto& job) {
        if (job.modIndex < 0 ||
            static_cast<size_t>(job.modIndex) >= g_managedHurtBindings.size() ||
            g_managedHurtBindings[static_cast<size_t>(job.modIndex)].modId != modId)
            return false;
        if (job.completion) {
            job.completion->cancelled = true;
            job.completion->done = true;
        }
        return true;
    });
    g_hurtScheduler.cancelledBeforeRun += cancelledHurt;
    if (cancelledHurt) g_managedHurtResultWake.notify_all();
    const auto cancelledRecipes = std::erase_if(g_recipeRpcQueue, [&](auto& job) {
        if (job.mod != modId) return false;
        if (job.completion) {
            job.completion->cancelled = true;
            job.completion->output = "PENDING|Stage=Unload Reason=ModRevoked";
            job.completion->done = true;
        }
        return true;
    });
    if (cancelledRecipes) g_recipeRpcWake.notify_all();

    for (size_t index = 0; index < g_entityBindings.size(); ++index) {
        auto& binding = g_entityBindings[index];
        if (binding.modId != modId || !binding.armed) continue;
        binding.armed = false;
        for (const auto& type : binding.npcTypes) {
            auto& owner = g_entityNpcTypeOwner[static_cast<size_t>(type.first)];
            if (owner.load(std::memory_order_acquire) == static_cast<int>(index + 1))
                owner.store(0, std::memory_order_release);
        }
        for (const auto& type : binding.projectileTypes) {
            auto& owner = g_entityProjectileTypeOwner[static_cast<size_t>(type.first)];
            if (owner.load(std::memory_order_acquire) == static_cast<int>(index + 1))
                owner.store(0, std::memory_order_release);
        }
    }
    for (const auto& job : g_entityQueue) {
        if (job.binding < 0 ||
            static_cast<size_t>(job.binding) >= g_entityBindings.size() ||
            g_entityBindings[static_cast<size_t>(job.binding)].modId != modId ||
            job.slot < 0) continue;
        if (job.kind == 0 && static_cast<size_t>(job.slot) < g_entityNpcPending.size())
            g_entityNpcPending[static_cast<size_t>(job.slot)] = false;
        if (job.kind == 1 &&
            static_cast<size_t>(job.slot) < g_entityProjectilePending.size())
            g_entityProjectilePending[static_cast<size_t>(job.slot)] = false;
    }
    eraseManagedModBoundJobs(g_entityQueue, g_entityBindings, modId);
    for (auto& result : g_entityNpcResults) {
        if (!result.ready || result.type < 0 || result.type >= 32768) continue;
        const int owner = g_entityNpcTypeOwner[static_cast<size_t>(result.type)].load(
            std::memory_order_acquire);
        if (owner != 0) continue;
        for (const auto& binding : g_entityBindings) {
            if (binding.modId != modId) continue;
            if (std::any_of(binding.npcTypes.begin(), binding.npcTypes.end(),
                [&](const auto& entry) { return entry.first == result.type; })) {
                result = {};
                break;
            }
        }
    }
    for (auto& result : g_entityProjectileResults) {
        if (!result.ready || result.type < 0 || result.type >= 32768) continue;
        const int owner = g_entityProjectileTypeOwner[static_cast<size_t>(result.type)].load(
            std::memory_order_acquire);
        if (owner != 0) continue;
        for (const auto& binding : g_entityBindings) {
            if (binding.modId != modId) continue;
            if (std::any_of(binding.projectileTypes.begin(), binding.projectileTypes.end(),
                [&](const auto& entry) { return entry.first == result.type; })) {
                result = {};
                break;
            }
        }
    }
}

void invalidateManagedLiveSourceMod(const std::string& modId) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_liveSourceRevocations.revoke(modId, g_sourceLiveEntries);
}

LiveSourceStatus liveSourceStatus(const std::string& modId) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_liveSourceRevocations.revoked(modId)) {
        LiveSourceStatus status;
        status.modId = modId;
        status.phase = "restart-required";
        status.error = "Live Mono assembly remains loaded after mod unload";
        return status;
    }
    const auto found = g_sourceLiveEntries.find(modId);
    if (found != g_sourceLiveEntries.end()) return found->second.status;
    LiveSourceStatus status;
    status.modId = modId;
    status.phase = g_domain && g_status.domainInitialized && g_status.methodExecuted
        ? "waiting-assembly" : "waiting-game-mono";
    return status;
}

LiveSourceStatus invokeLiveSourceTick(const std::string& modId,
                                     const std::string& assemblyPath,
                                     int tick, int state) {
    std::lock_guard<std::mutex> guard(g_mutex);
    LiveSourceStatus invalid;
    invalid.modId = modId;
    invalid.phase = "rejected";
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        tick < 0 || tick > 1000000 ||
        state < -1000000 || state > 1000000) {
        invalid.error = "Invalid mod identity or callback arguments";
        return invalid;
    }
    if (g_liveSourceRevocations.revoked(modId)) {
        invalid.phase = "restart-required";
        invalid.error = "Live Mono assembly remains loaded after mod unload";
        return invalid;
    }
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.invoke || !g_api.unbox) {
        invalid.phase = "waiting-game-mono";
        invalid.error = "Confirm managedGameProbe before trying live source callbacks";
        return invalid;
    }
    if (!validAssembly(assemblyPath)) {
        invalid.phase = "assembly-missing";
        invalid.error = "Mod assembly missing: compile the .bmod before the dynamic probe";
        return invalid;
    }
    auto existing = g_sourceLiveEntries.find(modId);
    if (existing == g_sourceLiveEntries.end()) {
        if (g_sourceLiveEntries.size() >= 16) {
            invalid.error = "Dynamic prototype limit: 16 loaded C# mods";
            return invalid;
        }
        existing = g_sourceLiveEntries.emplace(modId, LiveEntry{}).first;
        existing->second.status.modId = modId;
    }
    LiveEntry& entry = existing->second;
    auto& status = entry.status;
    if (entry.method && entry.path != assemblyPath) {
        status.phase = "restart-required";
        status.error = "Live assembly path changed; restart Bunny before reloading";
        ++status.failures;
        return status;
    }
    if (status.failures >= 3) {
        status.phase = "disabled";
        status.error = "Managed callback fault limit reached for this mod";
        return status;
    }
    if (!g_api.attach(g_domain)) {
        status.phase = "attach-failed";
        status.error = "mono_thread_attach failed";
        ++status.failures;
        return status;
    }
    if (!entry.method) {
        void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyLiveEntry") : nullptr;
        entry.method = klass ? g_api.findMethod(klass, "Tick", 2) : nullptr;
        if (!entry.method) {
            status.phase = "entry-missing";
            status.error = "Add public static BunnyLoader.Source.BunnyLiveEntry.Tick(int,int)";
            ++status.failures;
            return status;
        }
        entry.path = assemblyPath;
        status.loaded = true;
        status.ready = true;
        status.phase = "ready";
        status.error.clear();
    }
    int argTick = tick;
    int argState = state;
    void* args[] = { &argTick, &argState };
    void* exception = nullptr;
    void* boxed = g_api.invoke(entry.method, nullptr, args, &exception);
    void* value = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!value) {
        status.phase = "managed-exception";
        status.ready = false;
        status.error = "Tick threw or did not return an int";
        ++status.failures;
        return status;
    }
    int32_t result = 0;
    std::memcpy(&result, value, sizeof(result));
    status.lastTick = tick;
    status.lastState = state;
    status.lastResult = result;
    ++status.callbacks;
    status.phase = "ready";
    status.ready = true;
    status.error.clear();
    return status;
}

LiveCallbackResult invokeLiveSourceEvent(const std::string& modId,
                                         const std::string& assemblyPath,
                                         bool isKill, int tick, int damage, int npcIndex,
                                         const ManagedNpcSnapshot& npc,
                                         const ManagedPlayerSnapshot& player,
                                         const std::string& callbackClass) {
    LiveCallbackResult result;
    result.phase = "rejected";
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        tick < 0 || damage < 0 || damage > 100000 ||
        npcIndex < -1 || npcIndex >= 200 || (isKill && npcIndex != -1) ||
        !validAssembly(assemblyPath) || callbackClass.empty() || callbackClass.size() > 72 ||
        callbackClass.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
        result.error = "Invalid C# event parameters or assembly";
        return result;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_liveSourceRevocations.revoked(modId)) {
        result.phase = "restart-required";
        result.error = "Live Mono assembly remains loaded after mod unload";
        return result;
    }
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.invoke || !g_api.unbox) {
        result.phase = "waiting-mono";
        result.error = "MonoVM not initialized";
        return result;
    }
    if (!g_api.attach(g_domain)) {
        result.phase = "attach-failed";
        result.error = "mono_thread_attach failed";
        return result;
    }
    const std::string methodKey = callbackClass == "BunnyLiveEvents" ?
        modId : modId + ":" + callbackClass;
    auto entry = g_sourceLiveEntries.find(methodKey);
    if (entry == g_sourceLiveEntries.end()) {
        if (g_sourceLiveEntries.size() >= 32) {
            result.error = "CSharp live entry limit reached";
            return result;
        }
        entry = g_sourceLiveEntries.emplace(methodKey, LiveEntry{}).first;
        entry->second.status.modId = modId;
    }
    LiveEntry& target = entry->second;
    if (!target.path.empty() && target.path != assemblyPath) {
        result.phase = "restart-required";
        result.error = "Managed assembly changed; restart Bunny Loader";
        return result;
    }
    const bool cached = isKill ? target.killMethod != nullptr : target.hitMethodsResolved;
    if (cached) {
        ++g_liveMethodCacheHits;
    } else {
        ++g_liveMethodCacheMisses;
        ++g_liveAssemblyOpenAttempts;
        void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
        if (assembly) {
            ++g_liveAssemblyOpenSuccesses;
            g_liveAssemblyOpenPaths.insert(assemblyPath);
        }
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(
            image, "BunnyLoader.Source", callbackClass.c_str()) : nullptr;
        if (isKill) {
            target.killMethod = klass ? g_api.findMethod(klass, "OnKill", 2) : nullptr;
        } else if (klass) {
            target.hitMethod = g_api.findMethod(klass, "OnHitNPC", 2);
            target.hitV2Method = g_api.findMethod(klass, "OnHitNPCV2", 3);
            target.hitV3Method = g_api.findMethod(klass, "OnHitNPCV3", 7);
            target.hitV4Method = g_api.findMethod(klass, "OnHitNPCV4", 12);
            target.hitMethodsResolved = true;
        }
        if (target.hitMethod || target.hitV2Method || target.hitV3Method ||
            target.hitV4Method || target.killMethod)
            target.path = assemblyPath;
    }
    const ManagedHitCallbackChoice hitChoice = chooseManagedHitCallback(
        target.hitMethod, target.hitV2Method, npcIndex,
        target.hitV3Method, managedNpcSnapshotIsValid(npc),
        target.hitV4Method, managedPlayerSnapshotIsValid(player));
    const bool useV2 = !isKill && hitChoice.usesV2;
    const bool useV3 = !isKill && hitChoice.usesV3;
    const bool useV4 = !isKill && hitChoice.usesV4;
    void* method = isKill ? target.killMethod : hitChoice.method;
    if (!method) {
        result.phase = "entry-missing";
        result.error = isKill
            ? callbackClass + ".OnKill(int,int) not found"
            : callbackClass + ".OnHitNPC / OnHitNPCV2 / OnHitNPCV3 / OnHitNPCV4 not found";
        return result;
    }
    int call = tick;
    int value = damage;
    int targetIndex = npcIndex;
    void* argsLegacy[] = {&call, &value};
    void* argsV2[] = {&call, &value, &targetIndex};
    int npcType = npc.npcType;
    int npcLife = npc.npcLife;
    float npcX = npc.posX;
    float npcY = npc.posY;
    void* argsV3[] = {&call, &value, &targetIndex, &npcType, &npcLife, &npcX, &npcY};
    int playerIndex = player.playerIndex;
    int playerLife = player.life;
    int playerLifeMax = player.lifeMax;
    float playerX = player.posX;
    float playerY = player.posY;
    void* argsV4[] = {&call, &value, &targetIndex, &npcType, &npcLife,
                      &npcX, &npcY, &playerIndex, &playerLife, &playerLifeMax,
                      &playerX, &playerY};
    result.invoked = true;
    result.usedV2 = useV2;
    result.usedV3 = useV3;
    result.usedV4 = useV4;
    void* exception = nullptr;
    void* boxed = g_api.invoke(method, nullptr, useV4 ? argsV4 : useV3 ? argsV3 : useV2 ? argsV2 : argsLegacy, &exception);
    void* unboxed = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!unboxed) {
        result.phase = "managed-exception";
        result.error = isKill ? "OnKill CSharp threw" :
            useV4 ? "OnHitNPCV4 CSharp threw" :
            useV3 ? "OnHitNPCV3 CSharp threw" :
            useV2 ? "OnHitNPCV2 CSharp threw" : "OnHitNPC CSharp threw";
        return result;
    }
    std::memcpy(&result.value, unboxed, sizeof(result.value));
    result.ready = true;
    result.phase = "ready";
    return result;
}

bool invokeLiveSourceNpcAi(const std::string& modId,
                           const std::string& assemblyPath,
                           int playerDeltaX, int npcLife,
                           int* velocityTenths) {
    if (!velocityTenths || playerDeltaX < -100000 || playerDeltaX > 100000 ||
        npcLife <= 0 || npcLife > 1000000) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_liveSourceRevocations.revoked(modId) ||
        !g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
        !g_api.attach || !g_api.invoke || !g_api.unbox) return false;
    const auto found = g_sourceLiveEntries.find(modId);
    if (found == g_sourceLiveEntries.end()) return false;
    LiveEntry& entry = found->second;
    auto& status = entry.status;
    if (!entry.method || entry.path != assemblyPath || status.npcFailures >= 3)
        return false;
    if (!g_api.currentThread || !g_api.detachThread) {
        ++status.npcFailures;
        status.error = "MonoVM lacks mono_thread_current or mono_thread_detach";
        return false;
    }
    void* previouslyAttached = g_api.currentThread();
    void* attachedThread = g_api.attach(g_domain);
    if (!attachedThread) {
        ++status.npcFailures;
        status.error = "Live NPC MonoVM thread attach failed";
        return false;
    }
    struct ScopedNativeMonoThread {
        DetachFn detach;
        LiveSourceStatus& status;
        void* attached;
        ~ScopedNativeMonoThread() {
            if (!attached) return;
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                                "live NPC mono_thread_detach begin");
#endif
            detach(attached);
            ++status.npcThreadDetaches;
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "BunnyCSharp",
                                "live NPC mono_thread_detach finished");
#endif
        }
    } monoThreadScope{g_api.detachThread, status, previouslyAttached ? nullptr : attachedThread};
    if (monoThreadScope.attached) ++status.npcThreadAttaches;
    else ++status.npcThreadAlreadyAttached;
    if (!entry.npcAiMethod) {
        void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyLiveNpc") : nullptr;
        entry.npcAiMethod = klass ? g_api.findMethod(klass, "AI", 2) : nullptr;
        if (!entry.npcAiMethod) {
            ++status.npcFailures;
            status.error = "BunnyLiveNpc.AI(int,int) not found in C# assembly";
            return false;
        }
    }
    int argDelta = playerDeltaX;
    int argLife = npcLife;
    void* arguments[] = {&argDelta, &argLife};
    void* exception = nullptr;
    void* boxed = g_api.invoke(entry.npcAiMethod, nullptr, arguments, &exception);
    void* value = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!value) {
        ++status.npcFailures;
        status.error = "Managed BunnyLiveNpc.AI threw or returned non-int";
        return false;
    }
    int32_t result = 0;
    std::memcpy(&result, value, sizeof(result));
    if (result < -18 || result > 18) {
        ++status.npcFailures;
        status.error = "Managed NPC horizontal velocity exceeds safety range";
        return false;
    }
    *velocityTenths = result;
    ++status.npcCallbacks;
    status.npcLastLife = npcLife;
    status.npcLastDeltaX = playerDeltaX;
    status.npcLastVelocityTenths = result;
    status.error.clear();
    return true;
}

bool invokeManagedProjectileAI(int tick, int state, int* nextState) {
    if (!nextState || tick < 0 || tick >= 120) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_api.attach ||
        !g_api.invoke || !g_api.unbox) return false;
    if (!g_api.attach(g_domain)) return false;

    if (!g_liveAiMethod) {
        const size_t slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return false;
        const std::string assemblyPath =
            g_checkpointPath.substr(0, slash + 1) + "BunnyManagedProbe.dll";
        if (!validAssembly(assemblyPath)) return false;
        void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
        void* img = assembly ? g_api.image(assembly) : nullptr;
        void* klass = img ? g_api.findClass(img, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
        g_liveAiMethod = klass ? g_api.findMethod(klass, "AI", 2) : nullptr;
        if (!g_liveAiMethod) return false;
    }

    int tickArg = tick;
    int stateArg = state;
    void* args[] = {&tickArg, &stateArg};
    void* exception = nullptr;
    void* boxed = g_api.invoke(g_liveAiMethod, nullptr, args, &exception);
    if (exception || !boxed) return false;
    void* unboxed = g_api.unbox(boxed);
    if (!unboxed) return false;
    int next = 0;
    std::memcpy(&next, unboxed, sizeof(next));
    if (next != state + (tick % 7) - 3) return false;
    *nextState = next;
    return true;
}


namespace {
ManagedVisualQueue g_visualQueue;
ManagedVisualState g_visualState;
std::mutex g_visualStateMutex;
std::atomic<size_t> g_visualActiveCount{0};
}

size_t pollManagedVisualCommands(const std::string& assemblyPath, size_t budget) {
    if (budget == 0) return 0;
    std::unique_lock<std::mutex> guard(g_mutex, std::defer_lock);
    if (!guard.try_lock() || !g_domain || !g_status.domainInitialized ||
        !g_api.currentThread || !g_api.attach || !g_api.invoke ||
        !g_api.findClass || !g_api.findMethod) return 0;
    if (g_visualQueue.size() >= ManagedVisualQueue::Capacity) return 0;

    using Utf8Fn = char* (*)(void*);
    using FreeFn = void (*)(void*);
    static void* cachedDomain = nullptr;
    static void* cachedMethod = nullptr;
    static Utf8Fn cachedUtf8 = nullptr;
    static FreeFn cachedRelease = nullptr;
    if (cachedDomain != g_domain) {
        cachedDomain = g_domain;
        cachedMethod = nullptr;
        cachedUtf8 = nullptr;
        cachedRelease = nullptr;
    }
    if (!cachedMethod && !validAssembly(assemblyPath)) return 0;
    void* current = g_api.currentThread();
    const bool alreadyAttached = current != nullptr;
    void* attached = alreadyAttached ? current : g_api.attach(g_domain);
    if (!attached) return 0;
    struct DetachGuard {
        DetachFn fn;
        void* thread;
        ~DetachGuard() { if (fn && thread) fn(thread); }
    } detach{g_api.detachThread, alreadyAttached ? nullptr : attached};

    if (!cachedMethod) {
        const size_t separator = g_checkpointPath.find_last_of('/');
        if (separator == std::string::npos) return 0;
        const std::string sdkPath =
            g_checkpointPath.substr(0, separator + 1) + "BunnySourceSdk.dll";
        if (sdkPath != assemblyPath || !validAssembly(sdkPath)) return 0;
        Utf8Fn utf8 = resolve<Utf8Fn>("mono_string_to_utf8");
        FreeFn release = resolve<FreeFn>("mono_free");
        if (!utf8 || !release) return 0;
        void* assembly = g_api.openAssembly(g_domain, sdkPath.c_str());
        void* image = assembly ? g_api.image(assembly) : nullptr;
        void* klass = image ? g_api.findClass(image, "BunnyLoader.Source", "BunnyVisual") : nullptr;
        void* method = klass ? g_api.findMethod(klass, "TakeWire", 0) : nullptr;
        if (!method) return 0;
        cachedUtf8 = utf8;
        cachedRelease = release;
        cachedMethod = method;
    }
    ManagedVisualMonoApi api;
    api.invoke = g_api.invoke;
    api.utf8 = cachedUtf8;
    api.freeString = cachedRelease;
    api.takeWireMethod = cachedMethod;
    const auto stats = ManagedVisualMonoPump::poll(g_visualQueue, api, budget);
    return stats.accepted;
}

bool takeManagedVisualCommand(ManagedVisualCommand& command) {
    return g_visualQueue.pop(command);
}

bool postManagedVisualCommand(ManagedVisualCommand command) {
    return g_visualQueue.push(std::move(command));
}

size_t advanceManagedVisualState(uint64_t tick, size_t budget) {
    std::lock_guard<std::mutex> guard(g_visualStateMutex);
    g_visualState.advance(tick);
    size_t consumed = 0;
    ManagedVisualCommand command;
    if (budget > 64) budget = 64;
    while (consumed < budget && g_visualQueue.pop(command)) {
        g_visualState.apply(command, tick);
        ++consumed;
    }
    g_visualActiveCount.store(g_visualState.size(),std::memory_order_release);
    return consumed;
}

size_t activeManagedVisualCount() {
    return g_visualActiveCount.load(std::memory_order_acquire);
}

std::vector<ManagedVisualCommand> snapshotManagedVisualCommands(int playerIndex, size_t limit) {
    std::lock_guard<std::mutex> guard(g_visualStateMutex);
    return g_visualState.snapshot(playerIndex, limit);
}
void visitManagedVisualCommands(int playerIndex, void (*visitor)(const ManagedVisualCommand&,void*), void* context, size_t limit) {
    std::lock_guard<std::mutex> guard(g_visualStateMutex);
    g_visualState.visit(playerIndex,visitor,context,limit);
}

void clearManagedVisualCommands() {
    std::lock_guard<std::mutex> guard(g_visualStateMutex);
    g_visualQueue.clear();
    g_visualState.clear();
    g_visualActiveCount.store(0,std::memory_order_release);
}


}




