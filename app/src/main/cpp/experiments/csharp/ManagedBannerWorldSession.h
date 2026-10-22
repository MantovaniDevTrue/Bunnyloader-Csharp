#pragma once

#include "experiments/csharp/ManagedBannerWorldFile.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace bl::csharp::banners {

class WorldSession final {
public:
    DiskResult begin(uint64_t epoch, const std::string& path, bool cloud) {
        if (!epoch) return DiskResult::InvalidPath;
        std::lock_guard<std::mutex> lock(mutex_);
        if (ready_ && epoch_ == epoch && path_ == path && cloud_ == cloud)
            return status_;
        ready_ = false;
        epoch_ = epoch;
        path_.clear();
        retained_.clear();
        cloud_ = cloud;
        status_ = cloud ? DiskResult::UnsupportedCloud : DiskResult::InvalidPath;
        if (!validWorldPath(path) || cloud) return status_;
        path_ = path;
        WorldEntries fromDisk;
        status_ = readBannerWorldFile(path, false, &fromDisk);
        if (status_ == DiskResult::Ok || status_ == DiskResult::Missing) {
            retained_ = std::move(fromDisk);
            ready_ = true;
        }
        return status_;
    }

    void end() {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_ = false;
        epoch_ = 0;
        cloud_ = false;
        path_.clear();
        retained_.clear();
        status_ = DiskResult::InvalidPath;
    }

    bool view(uint64_t epoch, WorldEntries* result) const {
        if (!result) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_ || epoch != epoch_) return false;
        *result = retained_;
        return true;
    }

    bool commit(uint64_t epoch, bool authoritative, const WorldEntries& states) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_ || !authoritative || !epoch || epoch != epoch_ ||
            cloud_ || (status_ != DiskResult::Ok && status_ != DiskResult::Missing))
            return false;
        // A ponte do BannerSystem deve ter chamado WorldLedger::capture
        // sobre a copia de retained_ antes de chegar aqui. Este metodo nao
        // tenta obter estado do jogo fora da thread principal.
        const DiskResult result = writeBannerWorldFile(path_, false, states);
        if (result != DiskResult::Ok) return false;
        retained_ = states;
        status_ = DiskResult::Ok;
        return true;
    }

    DiskResult status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

private:
    mutable std::mutex mutex_;
    uint64_t epoch_ = 0;
    std::string path_;
    WorldEntries retained_;
    DiskResult status_ = DiskResult::InvalidPath;
    bool ready_ = false;
    bool cloud_ = false;
};

inline WorldSession& worldSession() {
    static WorldSession session;
    return session;
}

} // namespace bl::csharp::banners
