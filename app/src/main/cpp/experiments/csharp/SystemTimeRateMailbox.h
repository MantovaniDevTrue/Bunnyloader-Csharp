#pragma once
#include <cstdint>
#include <cmath>
#include <mutex>

namespace bl::csharp {
struct SystemTimeRates { double time=1, tiles=1, events=1; };
inline bool validSystemTimeRate(double value) {
    return std::isfinite(value) && value >= 0 && value <= 2147483647.0;
}

// The game thread never holds this lock while waiting for Mono. A late worker
// must not publish an old triple into a newly started frame's callback window.
class SystemTimeRateMailbox {
    std::mutex mutex;
    int64_t revision=0;
    bool live=false, modified=false;
    SystemTimeRates baseline{}, proposed{};
public:
    void begin(double time,double tiles) {
        std::lock_guard lock(mutex);
        baseline={time,tiles,time}; proposed=baseline;
        ++revision; modified=false; live=true;
    }
    bool read(int64_t* session,double* time,double* tiles,double* events) {
        if(!session || !time || !tiles || !events) return false;
        std::lock_guard lock(mutex);
        if(!live) return false;
        *session=revision;*time=baseline.time;*tiles=baseline.tiles;*events=baseline.events;
        return true;
    }
    bool write(int64_t session,double time,double tiles,double events) {
        if(!validSystemTimeRate(time) || !validSystemTimeRate(tiles) || !validSystemTimeRate(events)) return false;
        std::lock_guard lock(mutex);
        if(!live || session!=revision) return false;
        proposed={time,tiles,events};modified=true;
        return true;
    }
    SystemTimeRates finish() {
        std::lock_guard lock(mutex);
        live=false;
        return modified ? proposed : baseline;
    }
    void cancel() {
        std::lock_guard lock(mutex);
        live=false;modified=false;
    }
};
}
