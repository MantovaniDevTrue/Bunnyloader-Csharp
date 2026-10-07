#include "experiments/csharp/CSharpPrototype.h"

#include "core/Log.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <dlfcn.h>
#include <mutex>

namespace bl::csharp {

namespace {

using VersionFn = int (*)();
using AddFn = int (*)(int, int);
using ChecksumFn = int64_t (*)(int);
using PlayerSampleFn = int64_t (*)(float, float, int, int);

void* g_handle = nullptr;
VersionFn g_version = nullptr;
AddFn g_add = nullptr;
ChecksumFn g_checksum = nullptr;
PlayerSampleFn g_playerSample = nullptr;
bool g_attempted = false;
std::mutex g_lock;

template <typename T>
T symbol(const char* name) {
    return reinterpret_cast<T>(dlsym(g_handle, name));
}

__attribute__((noinline))
int cppAdd(int a, int b) {
    return a + b;
}

}

bool installPrototype() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_attempted) return g_handle != nullptr;
    g_attempted = true;

    g_handle = dlopen("libbunnycs.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_handle) {
        const char* error = dlerror();
        BL_WARN("csharp: libbunnycs.so nao carregou: %s", error ? error : "erro desconhecido");
        return false;
    }

    g_version = symbol<VersionFn>("bunnycs_version");
    g_add = symbol<AddFn>("bunnycs_add");
    g_checksum = symbol<ChecksumFn>("bunnycs_checksum");
    g_playerSample = symbol<PlayerSampleFn>("bunnycs_player_sample");

    if (!g_version || !g_add || !g_checksum || !g_playerSample) {
        BL_ERROR("csharp: exports NativeAOT incompletos");
        dlclose(g_handle);
        g_handle = nullptr;
        return false;
    }

    const int probe = g_add(20, 22);
    const int runtimeVersion = g_version();
    if (probe != 42 || runtimeVersion <= 0) {
        BL_ERROR("csharp: self-test falhou (version=%d add=%d)", runtimeVersion, probe);
        dlclose(g_handle);
        g_handle = nullptr;
        return false;
    }

    BL_INFO("csharp: NativeAOT carregado (ABI v%d, self-test 20+22=%d)", runtimeVersion, probe);

    const BenchmarkResult result = benchmark(250000);
    BL_INFO("csharp: benchmark direto cpp=%.1f ns/call csharp=%.1f ns/call ratio=%.2fx",
            result.cppNsPerCall, result.csharpNsPerCall, result.ratio);
    return true;
}

bool available() {
    return g_handle && g_version && g_add && g_checksum && g_playerSample;
}

int version() {
    return available() ? g_version() : 0;
}

int add(int a, int b) {
    return available() ? g_add(a, b) : 0;
}

int64_t checksum(int iterations) {
    return available() ? g_checksum(std::max(0, iterations)) : 0;
}

int64_t playerSample(float x, float y, int life, int maxLife) {
    return available() ? g_playerSample(x, y, life, maxLife) : 0;
}

BenchmarkResult benchmark(int iterations) {
    BenchmarkResult out;
    if (!available()) return out;
    iterations = std::clamp(iterations, 1000, 5000000);

    volatile int64_t cppValue = 0;
    auto cppStart = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        cppValue += cppAdd(i, 17);
    }
    auto cppEnd = std::chrono::steady_clock::now();

    volatile int64_t csharpValue = 0;
    auto csharpStart = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        csharpValue += g_add(i, 17);
    }
    auto csharpEnd = std::chrono::steady_clock::now();

    const double cppNs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(cppEnd - cppStart).count());
    const double csharpNs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(csharpEnd - csharpStart).count());

    out.cppNsPerCall = cppNs / iterations;
    out.csharpNsPerCall = csharpNs / iterations;
    out.ratio = out.cppNsPerCall > 0.0 ? out.csharpNsPerCall / out.cppNsPerCall : 0.0;
    out.cppValue = cppValue;
    out.csharpValue = csharpValue;
    return out;
}

}
