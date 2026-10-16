#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace bl::csharp {

struct ManagedProcMemory {
    int64_t rssKb = 0;
    int64_t peakRssKb = 0;
    int64_t virtualKb = 0;
    int64_t dataKb = 0;
    int64_t swapKb = 0;
    int threads = 0;
    bool available = false;
};

inline void parseManagedProcStatusLine(ManagedProcMemory& state,
                                       const char* line) {
    if (!line) return;
    long long kb = 0;
    int threads = 0;
    if (std::sscanf(line, "VmRSS: %lld kB", &kb) == 1) {
        state.rssKb = kb;
        state.available = true;
    } else if (std::sscanf(line, "VmHWM: %lld kB", &kb) == 1) {
        state.peakRssKb = kb;
    } else if (std::sscanf(line, "VmSize: %lld kB", &kb) == 1) {
        state.virtualKb = kb;
    } else if (std::sscanf(line, "VmData: %lld kB", &kb) == 1) {
        state.dataKb = kb;
    } else if (std::sscanf(line, "VmSwap: %lld kB", &kb) == 1) {
        state.swapKb = kb;
    } else if (std::sscanf(line, "Threads: %d", &threads) == 1) {
        state.threads = threads;
    }
}

inline ManagedProcMemory readManagedProcMemory() {
    ManagedProcMemory result;
    FILE* file = std::fopen("/proc/self/status", "rb");
    if (!file) return result;
    char line[256]{};
    while (std::fgets(line, sizeof(line), file))
        parseManagedProcStatusLine(result, line);
    std::fclose(file);
    return result;
}

}
