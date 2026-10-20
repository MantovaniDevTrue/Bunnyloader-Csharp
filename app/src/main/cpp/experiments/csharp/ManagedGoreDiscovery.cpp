#include "experiments/csharp/ManagedGoreDiscovery.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bl::csharp::gore {
namespace {

constexpr size_t kMaximumEntries = 32768;
constexpr size_t kMaximumGores = 256;
constexpr int kMaximumDepth = 20;
constexpr uint64_t kMaximumFileBytes = 16ull * 1024 * 1024;
constexpr uint64_t kMaximumTotalBytes = 32ull * 1024 * 1024;
constexpr uint32_t kMaximumDimension = 4096;

bool isGorePath(const std::string& path) {
    size_t cursor = 0;
    while (cursor < path.size()) {
        const size_t slash = path.find('/', cursor);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        if (path.substr(cursor, end - cursor) == "Gores" && end < path.size())
            return true;
        cursor = end + 1;
    }
    return false;
}

uint32_t readBigEndian(const unsigned char* bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool inspectPng(int dir, const std::string& name, Source* source,
                std::string* error) {
    const int fd = openat(dir, name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        *error = "nao foi possivel abrir gore: " + source->relativePath;
        return false;
    }
    struct stat metadata {};
    std::array<unsigned char, 33> header {};
    const bool regular = fstat(fd, &metadata) == 0 && S_ISREG(metadata.st_mode);
    const bool sizeOk = regular && metadata.st_size >= 33 &&
                        static_cast<uint64_t>(metadata.st_size) <= kMaximumFileBytes;
    size_t received = 0;
    if (sizeOk) {
        while (received < header.size()) {
            const ssize_t count = read(fd, header.data() + received,
                                       header.size() - received);
            if (count <= 0) break;
            received += static_cast<size_t>(count);
        }
    }
    close(fd);
    constexpr std::array<unsigned char, 8> signature{
        137, 80, 78, 71, 13, 10, 26, 10
    };
    if (!sizeOk || received != header.size() ||
        !std::equal(signature.begin(), signature.end(), header.begin()) ||
        readBigEndian(header.data() + 8) != 13 ||
        std::memcmp(header.data() + 12, "IHDR", 4) != 0) {
        *error = "PNG de gore ausente, truncado ou invalido: " + source->relativePath;
        return false;
    }
    source->width = readBigEndian(header.data() + 16);
    source->height = readBigEndian(header.data() + 20);
    source->byteCount = static_cast<uint64_t>(metadata.st_size);
    if (source->width == 0 || source->height == 0 ||
        source->width > kMaximumDimension ||
        source->height > kMaximumDimension) {
        *error = "dimensoes de gore fora do limite: " + source->relativePath;
        return false;
    }
    return true;
}

struct Collector {
    std::string root;
    std::vector<Source> files;
    std::map<std::string, std::string> names;
    std::string error;
    size_t visited = 0;
    uint64_t totalBytes = 0;

    bool walk(int parent, const std::string& relative, int depth) {
        if (depth > kMaximumDepth) {
            error = "pastas de gore excedem a profundidade permitida";
            return false;
        }
        const int copy = dup(parent);
        if (copy < 0) {
            error = "nao foi possivel abrir pasta de gore";
            return false;
        }
        DIR* dir = fdopendir(copy);
        if (!dir) {
            close(copy);
            error = "nao foi possivel listar pasta de gore";
            return false;
        }
        std::vector<std::string> entries;
        while (const dirent* item = readdir(dir)) {
            const std::string name = item->d_name;
            if (name == "." || name == ".." || name.empty() || name[0] == '.')
                continue;
            if (++visited > kMaximumEntries) {
                error = "pacote excede limite de entradas durante varredura de gores";
                closedir(dir);
                return false;
            }
            entries.push_back(name);
        }
        closedir(dir);
        std::sort(entries.begin(), entries.end());
        for (const auto& name : entries) {
            struct stat metadata {};
            if (fstatat(parent, name.c_str(), &metadata, AT_SYMLINK_NOFOLLOW) != 0) {
                error = "entrada removida durante varredura: " + name;
                return false;
            }
            // Links simbolicos sao ignorados: nem um PNG nem uma pasta podem
            // escapar da raiz do mod por meio de um link.
            if (S_ISLNK(metadata.st_mode)) continue;
            const std::string path = relative.empty() ? name : relative + "/" + name;
            if (S_ISDIR(metadata.st_mode)) {
                const int child = openat(parent, name.c_str(),
                    O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY);
                if (child < 0) {
                    error = "nao foi possivel abrir pasta: " + path;
                    return false;
                }
                const bool success = walk(child, path, depth + 1);
                close(child);
                if (!success) return false;
                continue;
            }
            if (!S_ISREG(metadata.st_mode) || name.size() <= 4 ||
                name.compare(name.size() - 4, 4, ".png") != 0 ||
                !isGorePath(path)) continue;
            if (files.size() >= kMaximumGores) {
                error = "limite de 256 gores por mod excedido";
                return false;
            }
            Source item;
            item.relativePath = path;
            item.absolutePath = root + "/" + path;
            item.name = name.substr(0, name.size() - 4);
            const auto inserted = names.emplace(item.name, path);
            if (!inserted.second) {
                error = "gore duplicado: " + item.name + " (" +
                    inserted.first->second + ", " + path + ")";
                return false;
            }
            if (!inspectPng(parent, name, &item, &error))
                return false;
            if (item.byteCount > kMaximumTotalBytes - totalBytes) {
                error = "gores do mod excedem 32 MiB";
                return false;
            }
            totalBytes += item.byteCount;
            files.push_back(std::move(item));
        }
        return true;
    }
};

}

bool discover(const std::string& modDirectory, std::vector<Source>* out,
              std::string* error) {
    if (!out || !error) return false;
    error->clear();
    if (modDirectory.empty() || modDirectory.size() > 4096) {
        *error = "pasta de mod C# invalida";
        return false;
    }
    Collector collector;
    collector.root = modDirectory;
    while (!collector.root.empty() && collector.root.back() == '/')
        collector.root.pop_back();
    collector.root += "/content";
    const int content = open(collector.root.c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY);
    if (content < 0) {
        *error = "pasta content do mod nao pode ser aberta";
        return false;
    }
    const bool ok = collector.walk(content, "", 0);
    close(content);
    if (!ok) {
        *error = collector.error;
        return false;
    }
    std::sort(collector.files.begin(), collector.files.end(),
        [](const Source& a, const Source& b) {
            return a.relativePath < b.relativePath;
        });
    *out = std::move(collector.files);
    return true;
}


bool readOwnedPng(const Source& source, std::vector<unsigned char>* bytes,
                  std::string* error) {
    if (!bytes || !error) return false;
    error->clear();
    const std::string suffix = "/" + source.relativePath;
    if (source.relativePath.empty() || source.absolutePath.size() <= suffix.size() ||
        source.absolutePath.compare(source.absolutePath.size() - suffix.size(),
                                    suffix.size(), suffix) != 0) {
        *error = "caminho de gore inconsistente";
        return false;
    }
    const std::string root = source.absolutePath.substr(
        0, source.absolutePath.size() - suffix.size());
    const int rootFd = open(root.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY);
    if (rootFd < 0) {
        *error = "raiz content do mod inacessivel";
        return false;
    }
    int current = rootFd;
    const std::string& path = source.relativePath;
    size_t start = 0;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const bool last = slash == std::string::npos;
        const std::string component = path.substr(
            start, last ? std::string::npos : slash - start);
        if (component.empty() || component == "." || component == ".." ||
            component.find('\\') != std::string::npos) {
            *error = "componente inseguro em " + path;
            close(current);
            return false;
        }
        const int next = openat(current, component.c_str(), O_RDONLY | O_CLOEXEC |
                                O_NOFOLLOW | (last ? 0 : O_DIRECTORY));
        close(current);
        if (next < 0) {
            *error = "gore alterada ou link simbolico: " + path;
            return false;
        }
        current = next;
        if (last) break;
        start = slash + 1;
    }
    struct stat metadata {};
    const bool valid = fstat(current, &metadata) == 0 && S_ISREG(metadata.st_mode) &&
                       metadata.st_size >= 33 &&
                       static_cast<uint64_t>(metadata.st_size) == source.byteCount &&
                       static_cast<uint64_t>(metadata.st_size) <= kMaximumFileBytes;
    if (!valid) {
        close(current);
        *error = "arquivo de gore alterado desde descoberta: " + path;
        return false;
    }
    std::vector<unsigned char> staged(static_cast<size_t>(metadata.st_size));
    size_t offset = 0;
    while (offset < staged.size()) {
        const ssize_t got = read(current, staged.data() + offset, staged.size() - offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        offset += static_cast<size_t>(got);
    }
    close(current);
    constexpr unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (offset != staged.size() ||
        !std::equal(std::begin(signature), std::end(signature), staged.begin()) ||
        readBigEndian(staged.data() + 8) != 13 ||
        std::memcmp(staged.data() + 12, "IHDR", 4) != 0 ||
        readBigEndian(staged.data() + 16) != source.width ||
        readBigEndian(staged.data() + 20) != source.height) {
        *error = "PNG de gore modificado durante carregamento: " + path;
        return false;
    }
    *bytes = std::move(staged);
    return true;
}

}
