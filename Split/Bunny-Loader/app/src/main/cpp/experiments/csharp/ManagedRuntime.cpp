#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ManagedLiveEventQueue.h"
#include "experiments/csharp/ManagedLiveIngressQueue.h"
#include "experiments/csharp/ManagedProcessMemory.h"
#include "experiments/csharp/MonoSignalOwnership.h"
#include "experiments/csharp/NativeAbortProbe.h"

#include <cstdio>
#include <algorithm>
#include <array>
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
};
LiveCallbackResult invokeLiveSourceEvent(const std::string& modId,
                                         const std::string& assemblyPath,
                                         bool isKill, int tick, int damage,
                                         const std::string& callbackClass = "BunnyLiveEvents");
ManagedRuntimeStatus probeManagedRuntimeStartupStage(
    const std::string& assemblyPath, const std::string& stage);

namespace {

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
std::string g_checkpointPath;
void* g_liveAiMethod = nullptr;

struct LiveEntry {
    std::string path;
    void* method = nullptr;
    void* npcAiMethod = nullptr;
    void* hitMethod = nullptr;
    void* killMethod = nullptr;
    LiveSourceStatus status;
};

std::map<std::string, LiveEntry> g_sourceLiveEntries;
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
           st.st_size > 0 && st.st_size <= 8 * 1024 * 1024;
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

std::mutex g_pinnedMutex;
std::condition_variable g_pinnedWake;
std::condition_variable g_pinnedResultWake;
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
    std::string error;
};

std::array<AdditionalLiveBinding, 7> g_additionalLiveBindings{};
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
std::array<PerfProjectileCounters, 8> g_perfProjectileCounters{};
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
    g_liveHitStatus.dropped += g_liveHitQueue.size();
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
                ManagedLiveQueuedEvent event;
                bool hasEvent = g_liveHitQueue.pop(&event);
                if (!hasEvent && g_liveIngress.pop(&event)) {
                    hasEvent = true;
                    event.tick = ++g_liveHitStatus.queued;
                    g_liveHitStatus.maxQueueDepth = std::max(
                        g_liveHitStatus.maxQueueDepth, g_liveIngress.maxDepth());
                    if (g_liveHitStatus.queued >= ManagedLiveEventQueue::kEventLimit)
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
                        ++binding->dropped;
                        ++g_liveHitStatus.dropped;
                        g_liveHitInFlight = false;
                        g_liveHitActiveBindingSlot = -2;
                        continue;
                    }
                    const std::string modId = binding ? binding->modId : g_liveHitStatus.modId;
                    const std::string dll = binding ? binding->assemblyPath : g_liveHitAssembly;
                    const std::string callbackClass = binding ?
                        binding->callbackClass : "BunnyLiveEvents";
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
                                                 event.damage, callbackClass)
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
                    if ((binding || g_liveHitStatus.modId == modId) &&
                        g_liveHitGeneration == generation && g_liveHitStatus.enabled) {
                        if (callback.ready && callback.phase == "ready") {
                            ++g_liveHitStatus.processed;
                            if (event.isKill) ++g_liveHitStatus.killProcessed;
                            else ++g_liveHitStatus.hitProcessed;
                            if (binding) {
                                ++binding->processed;
                                if (event.isKill) ++binding->kills;
                                else ++binding->hits;
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
                            ++g_liveHitStatus.failures;
                            g_liveHitStatus.phase = callback.phase;
                            g_liveHitStatus.error = callback.error;
                            if (binding) {
                                ++binding->failures;
                                binding->error = callback.error;
                                if (binding->failures >= 3)
                                    binding->armedType.store(-1, std::memory_order_release);
                            }
                            if (!binding) ++g_primaryLiveFailures;
                            if (!binding && g_primaryLiveFailures >= 3) {
                                g_liveHitType.store(-1, std::memory_order_release);
                                disarmAdditionalLiveBindings();
                                g_liveHitStatus.enabled = false;
                                g_liveHitStatus.dropped += g_liveHitQueue.size();
                                g_liveHitQueue.clear();
                                g_liveHitStatus.dropped += g_liveIngress.clear();
                                g_liveHitStatus.phase = "disabled";
                            }
                        }
                        if (g_liveHitStatus.enabled &&
                            g_liveHitStatus.queued >= ManagedLiveEventQueue::kEventLimit &&
                            g_liveHitQueue.empty() && g_liveIngress.depth() == 0) {
                            g_liveHitType.store(-1, std::memory_order_release);
                            disarmAdditionalLiveBindings();
                            g_liveHitStatus.enabled = false;
                            g_liveHitStatus.phase = "limit-reached";
                        }
                    }
                    continue;
                }
                if (!g_auditEnabled) {
                    if (g_liveHitStatus.enabled || g_liveIngress.depth() != 0)
                        g_pinnedWake.wait_for(lock, std::chrono::milliseconds(2));
                    else
                        g_pinnedWake.wait(lock);
                    continue;
                }
                const int seconds = kAuditSamples[g_auditIndex];
                const auto until = g_auditStart + std::chrono::seconds(seconds);
                const auto deadline = g_liveHitStatus.enabled
                    ? std::min(until, std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(2)) : until;
                if (g_pinnedWake.wait_until(lock, deadline, [] {
                    return g_pinnedPending || g_vmComparePending ||
                        !g_liveHitQueue.empty() || g_liveIngress.depth() != 0;
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

void setManagedCheckpointPath(const std::string& path) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (path.size() <= 2048 && path.find("..") == std::string::npos)
        g_checkpointPath = path;
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
    g_liveHitQueue.clear();
    g_liveIngress.reset();
    g_liveHitLockDrops.store(0, std::memory_order_relaxed);
    g_liveHitStatus = {};
    g_primaryLiveFailures = 0;
    g_liveHitStatus.automatic = automatic;
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
    status.queued = std::max(status.queued, g_liveIngress.accepted());
    status.queueDepth = g_liveHitQueue.size() + g_liveIngress.depth();
    status.maxQueueDepth = std::max(status.maxQueueDepth, g_liveIngress.maxDepth());
    status.inFlight = g_liveHitInFlight;
    status.lockDrops = g_liveHitLockDrops.load(std::memory_order_relaxed);
    status.dropped += status.lockDrops + g_liveIngress.dropped();
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
    g_liveHitStatus.queued = std::max(
        g_liveHitStatus.queued, g_liveIngress.accepted());
    g_liveHitStatus.dropped += g_liveHitQueue.size();
    g_liveHitQueue.clear();
    g_liveHitStatus.dropped += g_liveIngress.clear();
    g_liveHitStatus.enabled = false;
    g_liveHitStatus.phase = "paused";
    return g_liveHitStatus;
}

void queueManagedLiveEvent(int projectileType, int damage, bool isKill) {
    const auto admission = g_liveIngress.admission();
    if (damage < 0 || damage > 100000 || projectileType < 0 ||
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
            {0, damage, isKill, profiledAtNs, -1},
            projectileType, g_liveHitType, g_liveHitAutomatic);
    } else {
        AdditionalLiveBinding& binding =
            g_additionalLiveBindings[static_cast<size_t>(bindingSlot)];
        result = g_liveIngress.pushAdditional(admission,
            {0, damage, isKill, profiledAtNs, bindingSlot},
            projectileType, binding.armedType);
        if (result.accepted)
            binding.queued.fetch_add(1, std::memory_order_relaxed);
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

void queueManagedLiveHit(int projectileType, int damage) {
    queueManagedLiveEvent(projectileType, damage, false);
}

void queueManagedLiveKill(int projectileType, int damage) {
    queueManagedLiveEvent(projectileType, damage, true);
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
    if (!g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveBenchmark.running || g_liveBenchmarkAttempted ||
        g_liveHitStatus.queued >= ManagedLiveEventQueue::kEventLimit) {
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
        *error = "Maximum of seven additional bindings per session reached";
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
        entry.dropped += discarded;
        g_liveHitStatus.dropped += discarded;
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
    if (!g_liveHitStatus.enabled || !g_liveHitStatus.automatic ||
        g_liveHitType.load(std::memory_order_acquire) != g_liveHitStatus.projectileType ||
        g_liveHitStatus.phase == "draining" ||
        g_liveBenchmark.running || g_liveBenchmarkAttempted ||
        g_liveIngress.accepted() >= ManagedLiveEventQueue::kEventLimit) {
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
        status.lastResult = entry.lastResult;
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
    const std::string& diagnosticsPath) {
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
    const auto initialized = probeManagedRuntime(probeAssembly);
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
    installDestroyedMutexTrace();
    installNativeSegvTrace();
    trace("compiling-source");
    void* boxed = g_api.invoke(method, nullptr, args, &exception);
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

    trace("compiled-assembly-execution-start");
    void* verifierClass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "CompiledAssemblyProbe") : nullptr;
    void* verifier = verifierClass ? g_api.findMethod(verifierClass, "Verify", 2) : nullptr;
    if (!verifier) {
        out.phase = "compiled-assembly-verifier-missing";
        out.error = "CompiledAssemblyProbe.Verify indisponivel";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    void* verifyArgumentsManaged[] = {
        g_api.newString(g_domain, outputAssembly.c_str()),
        g_api.newString(g_domain, diagnosticsPath.c_str())
    };
    if (!verifyArgumentsManaged[0] || !verifyArgumentsManaged[1]) {
        out.phase = "compiled-assembly-verifier-arguments-failed";
        trace(out.phase.c_str());
        return out;
    }
    void* verifyArguments[] = { verifyArgumentsManaged[0], verifyArgumentsManaged[1] };
    void* verifyException = nullptr;
    void* verifiedBoxed = g_api.invoke(verifier, nullptr, verifyArguments, &verifyException);
    if (verifyException || !verifiedBoxed || !g_api.unbox(verifiedBoxed)) {
        out.phase = "compiled-assembly-verifier-exception";
        out.error = "Falha ao executar assembly C# compilado";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    std::memcpy(&out.executedTypes, g_api.unbox(verifiedBoxed), sizeof(out.executedTypes));
    if (out.executedTypes < 2 || out.executedTypes > 32) {
        out.phase = "compiled-assembly-execution-failed";
        out.error = "CompiledAssemblyProbe.Verify retornou quantidade de classes invalida";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    trace("compiled-assembly-execution-verified");
    out.phase = "compile-verified";
    trace(out.phase.c_str());
    return out;
}

LiveSourceStatus liveSourceStatus(const std::string& modId) {
    std::lock_guard<std::mutex> guard(g_mutex);
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
                                         bool isKill, int tick, int damage,
                                         const std::string& callbackClass) {
    LiveCallbackResult result;
    result.phase = "rejected";
    if (modId.empty() || modId.size() > 128 ||
        modId.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos ||
        tick < 0 || tick > ManagedLiveEventQueue::kEventLimit || damage < 0 || damage > 100000 ||
        !validAssembly(assemblyPath) || callbackClass.empty() || callbackClass.size() > 72 ||
        callbackClass.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
        result.error = "Invalid C# event parameters or assembly";
        return result;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
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
    void*& method = isKill ? target.killMethod : target.hitMethod;
    if (method) {
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
        method = klass ? g_api.findMethod(
            klass, isKill ? "OnKill" : "OnHitNPC", 2) : nullptr;
        if (!method) {
            result.phase = "entry-missing";
            result.error = isKill
                ? callbackClass + ".OnKill(int,int) not found"
                : callbackClass + ".OnHitNPC(int,int) not found";
            return result;
        }
        target.path = assemblyPath;
    }
    int call = tick;
    int value = damage;
    void* args[] = {&call, &value};
    void* exception = nullptr;
    void* boxed = g_api.invoke(method, nullptr, args, &exception);
    void* unboxed = exception || !boxed ? nullptr : g_api.unbox(boxed);
    if (!unboxed) {
        result.phase = "managed-exception";
        result.error = isKill ? "OnKill CSharp threw" : "OnHitNPC CSharp threw";
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
    if (!g_domain || !g_status.domainInitialized || !g_status.methodExecuted ||
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

}


