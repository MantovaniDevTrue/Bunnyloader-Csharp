#pragma once
#include <cstdint>

namespace bl::csharp::touch {

inline bool needsNativeTouchLayoutRefresh(bool restored, bool invalidated,
                                         uint64_t oldVersion, uint64_t version,
                                         const void* oldProfile,
                                         const void* profile) {
    return invalidated || !restored || oldVersion != version ||
           oldProfile != profile;
}

} // namespace bl::csharp::touch
