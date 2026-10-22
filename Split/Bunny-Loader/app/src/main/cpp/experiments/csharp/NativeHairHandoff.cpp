#include "experiments/csharp/NativeHairHandoff.h"

#include "experiments/csharp/NativeHairRegistry.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bl::csharp::hair {
namespace {

bool validDirectory(std::string_view path) {
    if (path.empty() || path.size() > 2048 || path.front() != '/' ||
        path.back() == '/' || path.find("//") != std::string_view::npos ||
        path.find("/../") != std::string_view::npos ||
        path.find("/./") != std::string_view::npos ||
        path.find('\\') != std::string_view::npos ||
        path.find('\0') != std::string_view::npos) return false;
    for (unsigned char c : path)
        if (c < 32 || c == 127) return false;
    return true;
}

}

std::string NativeHairManifestName(std::string_view uid) {
    return validIdentifier(uid) ? "source-hair-" + std::string(uid) + ".manifest"
                                : std::string{};
}

bool ReadNativeHairManifest(std::string_view workDir, std::string_view uid,
                            std::string* manifest, std::string* error) {
    if (!manifest || !error) return false;
    manifest->clear();
    error->clear();
    const std::string filename = NativeHairManifestName(uid);
    if (filename.empty() || !validDirectory(workDir)) {
        *error = "HairHandoffUnsafePathOrUid";
        return false;
    }

    const std::string directory(workDir);
    int dirfd = open(directory.c_str(), O_RDONLY | O_DIRECTORY |
                                         O_NOFOLLOW | O_CLOEXEC);
    if (dirfd < 0) {
        *error = "HairHandoffDirectoryUnavailable";
        return false;
    }
    const int file = openat(dirfd, filename.c_str(),
                            O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    const int fileError = errno;
    close(dirfd);
    if (file < 0) {
        *error = fileError == ENOENT ? "HairHandoffManifestMissing"
                                    : "HairHandoffManifestUnavailable";
        return false;
    }
    struct stat info{};
    constexpr size_t maxSize = 512 * 1024;
    if (fstat(file, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > static_cast<off_t>(maxSize)) {
        close(file);
        *error = "HairHandoffManifestInvalid";
        return false;
    }

    manifest->resize(static_cast<size_t>(info.st_size));
    size_t offset = 0;
    while (offset < manifest->size()) {
        const ssize_t received = read(file, manifest->data() + offset,
                                      manifest->size() - offset);
        if (received > 0) offset += static_cast<size_t>(received);
        else if (received == -1 && errno == EINTR) continue;
        else break;
    }
    const bool closed = close(file) == 0;
    if (!closed || offset != manifest->size() ||
        !manifest->starts_with("BUNNY_HAIR_MANIFEST_V1\n")) {
        manifest->clear();
        *error = "HairHandoffManifestTruncatedOrInvalid";
        return false;
    }
    return true;
}

} // namespace bl::csharp::hair
