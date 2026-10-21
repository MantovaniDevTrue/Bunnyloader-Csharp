#include "experiments/csharp/NativeCloudAutoload.h"

#include <algorithm>
#include <cstddef>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

constexpr int kMaxScanDepth = 8;
constexpr std::size_t kMaxFiles = 4096;

bool fail(std::string* error, const char* reason) {
    if (error) *error = reason;
    return false;
}

bool validName(const std::string& name) {
    if (name.empty() || name.size() > 72) return false;
    for (const char ch : name)
        if (!(ch >= 'A' && ch <= 'Z') &&
            !(ch >= 'a' && ch <= 'z') &&
            !(ch >= '0' && ch <= '9') && ch != '_')
            return false;
    return true;
}

// A single bounded walk over the mod content directory is enough. Never
// follow symlinks (including directories), and do not enumerate Android
// storage outside this mod's own content directory.
bool discover(const std::string& base, const std::string& relative,
              std::vector<std::string>* output, int depth,
              std::string* error) {
    if (depth > kMaxScanDepth || output->size() >= kMaxFiles)
        return fail(error, "CloudAutoload: scan depth or file limit exceeded");

    const std::string current = base + (relative.empty() ? "" : "/" + relative);
    DIR* stream = opendir(current.c_str());
    if (!stream)
        return depth == 0
            ? fail(error, "CloudAutoload: mod content directory unavailable")
            : true;

    while (dirent* item = readdir(stream)) {
        const std::string name(item->d_name);
        if (name == "." || name == "..") continue;
        if (name.size() > 128 || name.find('/') != std::string::npos)
            continue;
        const std::string path =
            relative.empty() ? name : relative + "/" + name;
        struct stat mode{};
        if (lstat((base + "/" + path).c_str(), &mode) != 0)
            continue;
        if (S_ISLNK(mode.st_mode)) continue;
        if (S_ISDIR(mode.st_mode)) {
            if (!discover(base, path, output, depth + 1, error)) {
                closedir(stream);
                return false;
            }
        } else if (S_ISREG(mode.st_mode) &&
                   path.size() > 4 &&
                   path.compare(path.size() - 4, 4, ".png") == 0) {
            // Detect /Clouds/ anywhere as in original ModFiles.ListTextures.
            const auto marker = path.find("Clouds/");
            if (marker != std::string::npos &&
                (marker == 0 || path[marker - 1] == '/')) {
                if (output->size() >= kMaxFiles) {
                    closedir(stream);
                    return fail(error, "CloudAutoload: too many textures");
                }
                output->push_back(path);
            }
        }
    }
    closedir(stream);
    return true;
}

bool classAlreadyRegistered(const NativeCloudRegistry& registry,
                            const std::string& modId,
                            const std::string& name) {
    const int first = registry.firstType();
    for (int index = 0; index < static_cast<int>(registry.count()); ++index) {
        const auto* entry = registry.find(first + index);
        if (entry && entry->modId == modId && entry->name == name)
            return true;
    }
    return false;
}

} // namespace

bool appendNativeCloudAutoload(NativeCloudRegistry* registry,
                               const std::string& modId,
                               const std::string& modDirectory,
                               std::string* error) {
    if (error) error->clear();
    if (!registry || registry->firstType() <= 0 || modId.empty() ||
        modDirectory.empty() || modDirectory.size() > 2048)
        return fail(error, "CloudAutoload: invalid registry or mod directory");

    std::vector<std::string> files;
    if (!discover(modDirectory + "/content", "", &files, 0, error))
        return false;
    std::sort(files.begin(), files.end());

    NativeCloudRegistry staged = *registry;
    for (const auto& path : files) {
        const auto slash = path.rfind('/');
        const std::string name = path.substr(
            slash == std::string::npos ? 0 : slash + 1,
            path.size() - (slash == std::string::npos ? 0 : slash + 1) - 4);
        if (!validName(name) || path.size() > 244) continue;
        if (classAlreadyRegistered(staged, modId, name)) continue;
        if (staged.add(modId, name, path.substr(0, path.size() - 4),
                       false) < 0)
            return fail(error, "CloudAutoload: registration capacity exhausted");
    }
    *registry = std::move(staged);
    return true;
}

} // namespace bl::csharp
