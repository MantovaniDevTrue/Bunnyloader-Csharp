#include "experiments/csharp/TileScanBenchmark.h"
#include "content/tiles/TileAccess.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

namespace bl::csharp {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int kSide = 512;
constexpr int kSmallTotal = kSide * kSide;
constexpr int kMaxCells = 24 * 1024 * 1024;
constexpr int kMinChunk = 8192;
constexpr int kMaxChunk = 1024 * 1024;
constexpr int64_t kBudgetUs = 2200;
constexpr uint64_t kOffset = 14695981039346656037ull;
constexpr uint64_t kPrime = 1099511628211ull;

enum class Phase { Idle, Capturing, Ready, Failed };

std::mutex g_mutex;
Phase g_phase = Phase::Idle;
std::unique_ptr<uint16_t[]> g_tiles;
bool g_fullWorld = false;
int g_copied = 0;
int g_total = 0;
int g_regionWidth = 0;
int g_regionHeight = 0;
int g_originX = 0;
int g_originY = 0;
int g_width = 0;
int g_height = 0;
const uint32_t* g_lookup = nullptr;
const uint16_t* g_type = nullptr;
const int16_t* g_header = nullptr;
uint64_t g_checksum = kOffset;
int64_t g_captureUs = 0;
int64_t g_wallUs = 0;
int g_frames = 0;
Clock::time_point g_begin{};

void clear() {
    g_phase = Phase::Idle;
    g_tiles.reset();
    g_fullWorld = false;
    g_copied = 0;
    g_total = 0;
    g_regionWidth = 0;
    g_regionHeight = 0;
    g_originX = 0;
    g_originY = 0;
    g_width = 0;
    g_height = 0;
    g_lookup = nullptr;
    g_type = nullptr;
    g_header = nullptr;
    g_checksum = kOffset;
    g_captureUs = 0;
    g_wallUs = 0;
    g_frames = 0;
    g_begin = {};
}

void fail() {
    g_phase = Phase::Failed;
    g_tiles.reset();
}

}

void pumpTileScanOnGameThread() {
    std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
    if (!lock || g_phase != Phase::Capturing) return;

    runtime::TileArrays world{};
    if (!runtime::tileArrays(&world) || !world.lookup || !world.type ||
        !world.sHeader || world.width < kSide || world.height < kSide) {
        fail();
        return;
    }

    if (g_total == 0) {
        const int64_t cells = static_cast<int64_t>(world.width) * world.height;
        if (cells <= 0 || cells > kMaxCells) {
            fail();
            return;
        }
        g_width = world.width;
        g_height = world.height;
        g_lookup = world.lookup;
        g_type = world.type;
        g_header = world.sHeader;
        g_regionWidth = g_fullWorld ? world.width : kSide;
        g_regionHeight = g_fullWorld ? world.height : kSide;
        g_originX = g_fullWorld ? 0 : (world.width - kSide) / 2;
        g_originY = g_fullWorld ? 0 : (world.height - kSide) / 2;
        g_total = g_regionWidth * g_regionHeight;
        g_tiles.reset(new (std::nothrow) uint16_t[g_total]);
        if (!g_tiles) {
            fail();
            return;
        }
        g_begin = Clock::now();
    } else if (g_width != world.width || g_height != world.height ||
               g_lookup != world.lookup || g_type != world.type ||
               g_header != world.sHeader) {
        fail();
        return;
    }

    const auto started = Clock::now();
    const int start = g_copied;
    const int maxEnd = g_copied + kMaxChunk < g_total
        ? g_copied + kMaxChunk : g_total;
    int i = start;
    while (i < maxEnd) {
        const int sliceEnd = i + kMinChunk < maxEnd ? i + kMinChunk : maxEnd;
        if (g_fullWorld) {
            for (; i < sliceEnd; ++i) {
                const uint32_t definition = world.lookup[i];
                const uint16_t value = (world.sHeader[definition] & runtime::kTileActiveBit)
                    ? world.type[definition] : UINT16_MAX;
                g_tiles[i] = value;
                g_checksum ^= value;
                g_checksum *= kPrime;
            }
        } else {
            for (; i < sliceEnd; ++i) {
                const int x = g_originX + i % g_regionWidth;
                const int y = g_originY + i / g_regionWidth;
                const int64_t offset = static_cast<int64_t>(world.width) * y + x;
                const uint32_t definition = world.lookup[offset];
                const uint16_t value = (world.sHeader[definition] & runtime::kTileActiveBit)
                    ? world.type[definition] : UINT16_MAX;
                g_tiles[i] = value;
                g_checksum ^= value;
                g_checksum *= kPrime;
            }
        }
        if (std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now() - started).count() >= kBudgetUs)
            break;
    }

    g_copied = i;
    ++g_frames;
    const auto finished = Clock::now();
    g_captureUs += std::chrono::duration_cast<std::chrono::microseconds>(
        finished - started).count();
    if (g_copied == g_total) {
        g_wallUs = std::chrono::duration_cast<std::chrono::microseconds>(
            finished - g_begin).count();
        g_phase = Phase::Ready;
    }
}

void resetTileScanOnGameThread() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_start_mode(int mode) {
    if (mode != 0 && mode != 1) return -3;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Capturing) return -1;
    clear();
    g_fullWorld = mode == 1;
    g_phase = Phase::Capturing;
    return mode == 0 ? kSmallTotal : 1;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_start() {
    return bunny_tile_scan_start_mode(0);
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_progress() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::Idle) return -1;
    if (g_phase == Phase::Failed) return -2;
    return g_copied;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_total() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_total;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_width() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_regionWidth;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_height() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_regionHeight;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_frames() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_frames;
}

extern "C" __attribute__((visibility("default"))) int bunny_tile_scan_read(
    uint16_t* destination, int capacity) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::Ready || !g_tiles || capacity != g_total ||
        !destination) return -2;
    std::memcpy(destination, g_tiles.get(), static_cast<size_t>(g_total) * sizeof(uint16_t));
    return g_total;
}

extern "C" __attribute__((visibility("default"))) uint64_t bunny_tile_scan_hash() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_phase == Phase::Ready ? g_checksum : 0;
}

extern "C" __attribute__((visibility("default"))) int64_t bunny_tile_scan_capture_us() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_phase == Phase::Ready ? g_captureUs : -1;
}

extern "C" __attribute__((visibility("default"))) int64_t bunny_tile_scan_wall_us() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_phase == Phase::Ready ? g_wallUs : -1;
}

extern "C" __attribute__((visibility("default"))) void bunny_tile_scan_stop() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear();
}

}
