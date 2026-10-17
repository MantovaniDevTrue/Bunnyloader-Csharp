#include "experiments/csharp/WorldMapBenchmark.h"
#include "content/common/GameRefs.h"
#include "il2cpp/Api.h"
#include "il2cpp/GuardedCall.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

namespace bl::csharp {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int kChunkSide = 64;
constexpr int kMaxCells = 24 * 1024 * 1024;
constexpr int kMaxChunksPerFrame = 24;
constexpr int64_t kBudgetUs = 1800;
constexpr uint64_t kOffset = 14695981039346656037ull;
constexpr uint64_t kPrime = 1099511628211ull;

enum class Phase { Idle, Capturing, Ready, Failed };

struct MapCell {
    uint16_t type;
    uint8_t light;
    uint8_t extra;
};
static_assert(sizeof(MapCell) == sizeof(uint32_t));

struct Refs {
    const MethodInfo* getMap = nullptr;
    const MethodInfo* getChunkTile = nullptr;
    int32_t width = -1;
    int32_t height = -1;
    int32_t tileData = -1;
    bool ready = false;
};

Refs& refs() {
    static Refs r;
    static std::once_flag once;
    std::call_once(once, [] {
        auto& api = il2cpp::api();
        auto* main = il2cpp::findClass({"Terraria", "Main", {}});
        auto* map = il2cpp::findClass({"Terraria.Map", "WorldMap", {}});
        auto* chunk = il2cpp::findClass({"Terraria.Map", "WorldMapChunk", {}});
        r.getMap = main ? api.class_get_method_from_name(main, "get_Map", 0) : nullptr;
        r.getChunkTile = map ? api.class_get_method_from_name(map, "GetChunkTile", 3) : nullptr;
        r.width = map ? il2cpp::fieldOffset(map, "MaxWidth") : -1;
        r.height = map ? il2cpp::fieldOffset(map, "MaxHeight") : -1;
        r.tileData = chunk ? il2cpp::fieldOffset(chunk, "TileData") : -1;
        r.ready = r.getMap && r.getChunkTile && r.width >= 0 &&
                  r.height >= 0 && r.tileData >= 0 &&
                  api.monitor_enter && api.monitor_exit;
    });
    return r;
}

std::mutex g_mutex;
Phase g_phase = Phase::Idle;
int g_error = 0;
std::unique_ptr<uint32_t[]> g_cells;
Il2CppObject* g_map = nullptr;
int g_width = 0;
int g_height = 0;
int g_total = 0;
int g_nextChunk = 0;
int g_chunkColumns = 0;
int g_chunkCount = 0;
int g_done = 0;
int g_frames = 0;
int64_t g_workUs = 0;
int64_t g_wallUs = 0;
Clock::time_point g_firstFrame{};

void clear() {
    g_phase = Phase::Idle;
    g_error = 0;
    g_cells.reset();
    g_map = nullptr;
    g_width = 0;
    g_height = 0;
    g_total = 0;
    g_nextChunk = 0;
    g_chunkColumns = 0;
    g_chunkCount = 0;
    g_done = 0;
    g_frames = 0;
    g_workUs = 0;
    g_wallUs = 0;
    g_firstFrame = {};
}

void fail(int code) {
    g_error = code;
    g_phase = Phase::Failed;
    g_cells.reset();
}

bool getMap(Il2CppObject** result) {
    const auto* method = refs().getMap;
    auto body = [&] {
        auto fn = reinterpret_cast<Il2CppObject* (*)(const MethodInfo*)>(
            il2cpp::methodPointer(method));
        *result = fn(method);
    };
    return il2cpp::guarded(body) && *result != nullptr;
}

bool getChunk(Il2CppObject* map, int x, int y, Il2CppObject** result) {
    const auto* method = refs().getChunkTile;
    MapCell* cell = nullptr;
    auto body = [&] {
        auto fn = reinterpret_cast<MapCell* (*)(Il2CppObject*, int, int,
                                                Il2CppObject**, const MethodInfo*)>(
            il2cpp::methodPointer(method));
        cell = fn(map, x, y, result, method);
    };
    return il2cpp::guarded(body) && cell && *result;
}

}

void pumpWorldMapBenchmarkOnGameThread() {
    std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
    if (!lock || g_phase != Phase::Capturing) return;

    const Refs& r = refs();
    if (!r.ready) {
        fail(-3);
        return;
    }

    Il2CppObject* map = nullptr;
    if (!getMap(&map)) {
        fail(-4);
        return;
    }

    const int width = runtime::field<int32_t>(map, r.width);
    const int height = runtime::field<int32_t>(map, r.height);
    const int64_t count = static_cast<int64_t>(width) * height;
    if (width < 1 || height < 1 || count < 1 || count > kMaxCells) {
        fail(-5);
        return;
    }

    if (!g_cells) {
        g_width = width;
        g_height = height;
        g_total = static_cast<int>(count);
        g_chunkColumns = (width + kChunkSide - 1) / kChunkSide;
        g_chunkCount = g_chunkColumns * ((height + kChunkSide - 1) / kChunkSide);
        g_cells.reset(new (std::nothrow) uint32_t[g_total]);
        if (!g_cells) {
            fail(-6);
            return;
        }
        g_map = map;
        g_firstFrame = Clock::now();
    } else if (g_map != map || g_width != width || g_height != height) {
        fail(-7);
        return;
    }

    const auto started = Clock::now();
    int chunksThisFrame = 0;
    while (g_nextChunk < g_chunkCount && chunksThisFrame < kMaxChunksPerFrame) {
        const int cx = g_nextChunk % g_chunkColumns;
        const int cy = g_nextChunk / g_chunkColumns;
        const int x = cx * kChunkSide;
        const int y = cy * kChunkSide;
        const int cols = std::min(kChunkSide, g_width - x);
        const int rows = std::min(kChunkSide, g_height - y);
        Il2CppObject* chunk = nullptr;
        if (!getChunk(map, x, y, &chunk)) {
            fail(-8);
            return;
        }
        auto& api = il2cpp::api();
        api.monitor_enter(chunk);
        const auto* data = runtime::field<MapCell*>(chunk, r.tileData);
        if (data) {
            for (int dy = 0; dy < rows; ++dy) {
                uint32_t* destination = g_cells.get() +
                    static_cast<size_t>(y + dy) * g_width + x;
                std::memcpy(destination, data + dy * kChunkSide,
                            static_cast<size_t>(cols) * sizeof(MapCell));
            }
        }
        api.monitor_exit(chunk);
        if (!data) {
            fail(-9);
            return;
        }

        g_done += cols * rows;
        ++g_nextChunk;
        ++chunksThisFrame;

        if (std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now() - started).count() >= kBudgetUs)
            break;
    }

    ++g_frames;
    const auto finished = Clock::now();
    g_workUs += std::chrono::duration_cast<std::chrono::microseconds>(
        finished - started).count();
    if (g_nextChunk >= g_chunkCount) {
        g_wallUs = std::chrono::duration_cast<std::chrono::microseconds>(
            finished - g_firstFrame).count();
        g_phase = Phase::Ready;
    }
}

void resetWorldMapBenchmarkOnGameThread() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

extern "C" __attribute__((visibility("default"))) int bunny_worldmap_bench_start() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Capturing) return -1;
    clear();
    g_phase = Phase::Capturing;
    return 1;
}

extern "C" __attribute__((visibility("default"))) int bunny_worldmap_bench_progress() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Idle) return -1;
    if (g_phase == Phase::Failed) return g_error;
    return g_done;
}

extern "C" __attribute__((visibility("default"))) int bunny_worldmap_bench_stat(int key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    switch (key) {
        case 0: return g_total;
        case 1: return g_width;
        case 2: return g_height;
        case 3: return g_frames;
        case 4: return g_nextChunk;
        case 5: return g_chunkCount;
        case 6: return g_phase == Phase::Ready ? 1 : 0;
        default: return -1;
    }
}

extern "C" __attribute__((visibility("default"))) int64_t bunny_worldmap_bench_time(int key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::Ready) return -1;
    return key == 0 ? g_wallUs : key == 1 ? g_workUs : -1;
}

extern "C" __attribute__((visibility("default"))) int bunny_worldmap_bench_read(
    uint32_t* destination, int capacity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::Ready || !destination ||
        capacity != g_total || !g_cells) return -2;
    std::memcpy(destination, g_cells.get(),
                static_cast<size_t>(g_total) * sizeof(uint32_t));
    return g_total;
}

extern "C" __attribute__((visibility("default"))) uint64_t bunny_worldmap_bench_hash() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::Ready || !g_cells) return 0;
    uint64_t hash = kOffset;
    for (int i = 0; i < g_total; ++i) {
        hash ^= g_cells[i];
        hash *= kPrime;
    }
    return hash;
}

extern "C" __attribute__((visibility("default"))) void bunny_worldmap_bench_stop() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

}
