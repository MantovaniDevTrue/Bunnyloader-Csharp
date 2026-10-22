#include "experiments/csharp/NativeHairSidecar.h"

#include "experiments/csharp/NativeHairRegistry.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bl::csharp::hair {
namespace {

constexpr std::string_view kHeader = "BUNNY_CSHARP_HAIR_V1\n";
std::atomic<uint64_t> sequence{0};

bool validKey(std::string_view key) {
    return key.starts_with("H1|") &&
           splitName(key.substr(3), nullptr, nullptr);
}

bool validPath(std::string_view path) {
    if (path.empty() || path.size() > 3072 || path.front() != '/' ||
        !path.ends_with(".plr") || path.find('\0') != std::string_view::npos ||
        path.find("/../") != std::string_view::npos ||
        path.find("/./") != std::string_view::npos ||
        path.find("//") != std::string_view::npos) return false;
    for (const unsigned char c : path)
        if (c < 32 || c == 127) return false;
    return true;
}

std::string tempPath(const std::string& destination) {
    return destination + "." + std::to_string(getpid()) + "." +
           std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
}

bool writeComplete(int fd, std::string_view payload) {
    size_t done = 0;
    while (done < payload.size()) {
        const ssize_t written = ::write(fd, payload.data() + done, payload.size() - done);
        if (written > 0) done += static_cast<size_t>(written);
        else if (written < 0 && errno == EINTR) continue;
        else return false;
    }
    return true;
}

} // namespace

std::string NativeHairSidecarPath(std::string_view terrariaPlayerPath) {
    return validPath(terrariaPlayerPath)
        ? std::string(terrariaPlayerPath) + ".bl.csharp-hair"
        : std::string{};
}

bool WriteNativeHairSidecar(std::string_view terrariaPlayerPath,
                            std::string_view stableKey, std::string* error) {
    if (!error) return false;
    error->clear();
    const std::string target = NativeHairSidecarPath(terrariaPlayerPath);
    if (target.empty() || !validKey(stableKey) || stableKey.size() > 300) {
        *error = "HairSidecarInvalidPathOrKey";
        return false;
    }
    const std::string temporary = tempPath(target);
    int fd = open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        *error = "HairSidecarOpenFailed";
        return false;
    }
    const std::string data = std::string(kHeader) + std::string(stableKey) + "\n";
    const bool written = writeComplete(fd, data) && fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (!written || !closed) {
        unlink(temporary.c_str());
        *error = "HairSidecarWriteFailed";
        return false;
    }
    if (rename(temporary.c_str(), target.c_str()) != 0) {
        unlink(temporary.c_str());
        *error = "HairSidecarRenameFailed";
        return false;
    }
    return true;
}

SidecarRead ReadNativeHairSidecar(std::string_view terrariaPlayerPath,
                                  std::string* stableKey, std::string* error) {
    if (!stableKey || !error) return SidecarRead::Invalid;
    stableKey->clear();
    error->clear();
    const std::string path = NativeHairSidecarPath(terrariaPlayerPath);
    if (path.empty()) {
        *error = "HairSidecarInvalidPlayerPath";
        return SidecarRead::Invalid;
    }
    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) return SidecarRead::Missing;
        *error = "HairSidecarOpenFailed";
        return SidecarRead::IOError;
    }
    struct stat statbuf{};
    if (fstat(fd, &statbuf) != 0 || !S_ISREG(statbuf.st_mode) ||
        statbuf.st_size <= 0 || statbuf.st_size > 512) {
        close(fd);
        *error = "HairSidecarFileInvalid";
        return SidecarRead::Invalid;
    }
    std::string data(static_cast<size_t>(statbuf.st_size), '\0');
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t count = read(fd, data.data() + offset, data.size() - offset);
        if (count > 0) offset += static_cast<size_t>(count);
        else if (count < 0 && errno == EINTR) continue;
        else break;
    }
    const bool closed = close(fd) == 0;
    if (offset != data.size() || !closed) {
        *error = "HairSidecarReadFailed";
        return SidecarRead::IOError;
    }
    if (!data.starts_with(kHeader) || data.back() != '\n') {
        *error = "HairSidecarHeaderInvalid";
        return SidecarRead::Invalid;
    }
    std::string_view key(data);
    key.remove_prefix(kHeader.size());
    key.remove_suffix(1);
    if (!validKey(key)) {
        *error = "HairSidecarKeyInvalid";
        return SidecarRead::Invalid;
    }
    stableKey->assign(key);
    return SidecarRead::Found;
}

} // namespace bl::csharp::hair
