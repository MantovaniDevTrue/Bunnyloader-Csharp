#include "experiments/csharp/TileScanBenchmark.h"
#include "content/tiles/TileAccess.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace bl::csharp {
namespace {

constexpr int kSide = 512;
constexpr int kTotal = kSide * kSide;
constexpr int kCellsPerFrame = 8192;
constexpr uint64_t kOffset = 14695981039346656037ull;
constexpr uint64_t kPrime = 1099511628211ull;

enum class Phase { Idle, Capturing, Ready, Failed };

std::mutex g_mutex;
Phase g_phase = Phase::Idle;
std::vector<uint16_t> g_tiles;
int g_copied = 0;
int g_originX = 0;
int g_originY = 0;
int g_width = 0;
int g_height = 0;
const uint32_t* g_lookup = nullptr;
uint64_t g_checksum = kOffset;
int64_t g_captureUs = 0;

void clear() {
    g_phase = Phase::Idle;
    g_tiles.clear();
    g_copied = 0;
    g_originX = 0;
    g_originY = 0;
    g_width = 0;
    g_height = 0;
    g_lookup = nullptr;
    g_checksum = kOffset;
    g_captureUs = 0;
}

}

void pumpTileScanOnGameThread() {
    std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
    if (!lock || g_phase != Phase::Capturing) return;
    const auto started = std::chrono::steady_clock::now();

    runtime::TileArrays world{};
    if (!runtime::tileArrays(&world) || !world.lookup || !world.type ||
        !world.sHeader || world.width < kSide || world.height < kSide) {
        g_phase = Phase::Failed;
        return;
    }

    if (g_copied == 0) {
        g_width = world.width;
        g_height = world.height;
        g_lookup = world.lookup;
        g_originX = (world.width - kSide) / 2;
        g_originY = (world.height - kSide) / 2;
    } else if (g_width != world.width || g_height != world.height ||
               g_lookup != world.lookup) {
        g_phase = Phase::Failed;
        return;
    }

    const int limit = g_copied + kCellsPerFrame < kTotal
        ? g_copied + kCellsPerFrame : kTotal;
    for (int i = g_copied; i < limit; ++i) {
        const int x = g_originX + i % kSide;
        const int y = g_originY + i / kSide;
        const int64_t offset = static_cast<int64_t>(world.width) * y + x;
        const uint32_t definition = world.lookup[offset];
        const uint16_t value = (world.sHeader[definition] & runtime::kTileActiveBit)
            ? world.type[definition] : UINT16_MAX;
        g_tiles[i] = value;
        g_checksum ^= value;
        g_checksum *= kPrime;
    }
    g_copied = limit;
    g_captureUs += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count();
    if (g_copied == kTotal) g_phase = Phase::Ready;
}

void resetTileScanOnGameThread() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_start() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Capturing) return -1;
    clear();
    g_tiles.assign(kTotal, UINT16_MAX);
    g_phase = Phase::Capturing;
    return kTotal;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_progress() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Idle) return -1;
    if (g_phase == Phase::Failed) return -2;
    return g_copied;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_read(
    uint16_t* destination, int capacity) {
    if (!destination || capacity != kTotal) return -3;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::Ready || g_tiles.size() != kTotal) return -2;
    std::memcpy(destination, g_tiles.data(), kTotal * sizeof(uint16_t));
    return kTotal;
}

extern "C" __attribute__((visibility("default"))) uint64_t bunny_tile_scan_hash() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_phase == Phase::Ready ? g_checksum : 0;
}

extern "C" __attribute__((visibility("default"))) int64_t bunny_tile_scan_capture_us() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_phase == Phase::Ready ? g_captureUs : -1;
}

extern "C" __attribute__((visibility("default"))) void bunny_tile_scan_stop() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

}
