#pragma once

#include "experiments/csharp/ManagedBannerWorldState.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bl::csharp::banners {

enum class DiskResult {
    Ok,
    Missing,
    InvalidPath,
    UnsupportedCloud,
    Corrupt,
    IoError,
};

inline bool validWorldPath(const std::string& path) {
    if (path.size() < 5 || path.size() > 2048 ||
        path.substr(path.size() - 4) != ".wld" ||
        path.front() != '/' || path.find("..") != std::string::npos ||
        path.find('\\') != std::string::npos ||
        path.find('\0') != std::string::npos) return false;
    for (char c : path)
        if (static_cast<unsigned char>(c) < 32) return false;
    return true;
}

inline std::string bannerSidecarPath(const std::string& worldPath) {
    return validWorldPath(worldPath) ? worldPath + ".banners.bl" : std::string();
}

inline DiskResult readBannerWorldFile(const std::string& worldPath, bool cloud,
                                     WorldEntries* output) {
    if (!output || !validWorldPath(worldPath)) return DiskResult::InvalidPath;
    if (cloud) return DiskResult::UnsupportedCloud;
    const std::string path = bannerSidecarPath(worldPath);
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return errno == ENOENT ? DiskResult::Missing : DiskResult::IoError;
    struct stat meta{};
    if (fstat(fd, &meta) != 0 || !S_ISREG(meta.st_mode) ||
        meta.st_size < 10 ||
        static_cast<uintmax_t>(meta.st_size) > WorldLedger::kMaxBytes) {
        close(fd);
        return DiskResult::Corrupt;
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(meta.st_size));
    size_t done = 0;
    bool ioError = false;
    while (done < buffer.size()) {
        const ssize_t count = read(fd, buffer.data() + done, buffer.size() - done);
        if (count > 0) done += static_cast<size_t>(count);
        else if (count < 0 && errno == EINTR) continue;
        else { ioError = true; break; }
    }
    uint8_t extra = 0;
    ssize_t tail = -1;
    if (!ioError) {
        do { tail = read(fd, &extra, 1); } while (tail < 0 && errno == EINTR);
    }
    const int closeResult = close(fd);
    if (ioError || tail < 0 || closeResult != 0) return DiskResult::IoError;
    if (tail > 0) return DiskResult::Corrupt;
    WorldEntries staged;
    if (!WorldLedger::decode(buffer, &staged)) return DiskResult::Corrupt;
    *output = std::move(staged);
    return DiskResult::Ok;
}

inline DiskResult writeBannerWorldFile(const std::string& worldPath, bool cloud,
                                      const WorldEntries& entries) {
    if (!validWorldPath(worldPath)) return DiskResult::InvalidPath;
    if (cloud) return DiskResult::UnsupportedCloud;
    std::vector<uint8_t> binary;
    if (!WorldLedger::encode(entries, &binary)) return DiskResult::Corrupt;
    const std::string finalPath = bannerSidecarPath(worldPath);
    std::string tempPath = finalPath + ".tmpXXXXXX";
    std::vector<char> mutablePath(tempPath.begin(), tempPath.end());
    mutablePath.push_back('\0');
    const int fd = mkstemp(mutablePath.data());
    if (fd < 0) return DiskResult::IoError;
    bool ok = fchmod(fd, 0600) == 0;
    size_t done = 0;
    while (ok && done < binary.size()) {
        const ssize_t amount = write(fd, binary.data() + done, binary.size() - done);
        if (amount > 0) done += static_cast<size_t>(amount);
        else if (amount < 0 && errno == EINTR) continue;
        else ok = false;
    }
    if (ok) ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(mutablePath.data(), finalPath.c_str()) == 0;
    if (!ok) {
        unlink(mutablePath.data());
        return DiskResult::IoError;
    }
    return DiskResult::Ok;
}

} // namespace bl::csharp::banners
