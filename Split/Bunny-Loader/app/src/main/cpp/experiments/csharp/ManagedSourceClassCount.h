#pragma once

#include "experiments/csharp/SourceLoader.h"
#include <string>
#include <limits>

namespace bl::csharp {

inline int expectedManagedSourceTypes(const SourceModStatus& mod) {
    return mod.items + mod.tiles + mod.projectiles + mod.buffs +
           mod.players + mod.npcs + mod.prefixes;
}

inline bool sourceStatusCount(const std::string& status, const std::string& key, int* value) {
    const std::string marker = "\n" + key + "=";
    const size_t at = status.find(marker);
    if (at == std::string::npos) return false;
    const size_t start = at + marker.size(), end = status.find('\n', start);
    if (end == std::string::npos || end == start || end - start > 5 ||
        status.find(marker, start) != std::string::npos) return false;
    int count = 0;
    for (size_t i = start; i < end; ++i) {
        if (status[i] < '0' || status[i] > '9') return false;
        count = count * 10 + status[i] - '0';
    }
    if (count > 8192) return false;
    *value = count;
    return true;
}

inline bool managedSourceCompileFinished(const std::string& status,
                                         const std::string& modId,
                                         const SourceModStatus& mod) {
    int executed = 0, managed = 0;
    if (!sourceStatusCount(status, "executedTypes", &executed)) return false;
    if (status.find("\nmanagedContentTypes=") != std::string::npos) {
        if (!sourceStatusCount(status, "managedContentTypes", &managed)) return false;
    }
    return status.find("state=finished\n") != std::string::npos &&
           status.find("modId=" + modId + "\n") != std::string::npos &&
           status.find("phase=compile-verified\n") != std::string::npos &&
           status.find("assemblyReady=1\n") != std::string::npos &&
           executed > 0 && executed - managed == expectedManagedSourceTypes(mod) &&
           (mod.managedContentDeclarations == 0 || managed > 0);
}

}
