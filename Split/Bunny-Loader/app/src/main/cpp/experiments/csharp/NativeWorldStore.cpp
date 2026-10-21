#include "experiments/csharp/NativeWorldStore.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace bl::csharp::worldsave {
namespace {

constexpr size_t kMaxDocument = 1024 * 1024;
constexpr size_t kMaxName = 160;
constexpr size_t kMaxText = 4096;
constexpr uint16_t kMaxSystems = 512;
constexpr uint16_t kMaxFields = 128;
constexpr char kSuffix[] = ".csharp.bwc";
std::atomic<uint64_t> g_tempCounter{0};

bool readU16(const uint8_t* data, size_t end, size_t* position, uint16_t* result) {
    if (*position > end || end - *position < 2) return false;
    *result = static_cast<uint16_t>(data[*position]) |
              (static_cast<uint16_t>(data[*position + 1]) << 8);
    *position += 2;
    return true;
}

bool utf8Valid(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<uint8_t>(text[i]);
        if (c < 0x80) { ++i; continue; }
        int extra = 0;
        uint32_t point = 0;
        if (c >= 0xC2 && c <= 0xDF) { extra = 1; point = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { extra = 2; point = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { extra = 3; point = c & 0x07; }
        else return false;
        if (i + static_cast<size_t>(extra) >= text.size()) return false;
        for (int j = 1; j <= extra; ++j) {
            const auto b = static_cast<uint8_t>(text[i + static_cast<size_t>(j)]);
            if ((b & 0xC0) != 0x80) return false;
            point = (point << 6) | (b & 0x3F);
        }
        if (point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF) ||
            (extra == 2 && point < 0x800) || (extra == 3 && point < 0x10000))
            return false;
        i += 1 + static_cast<size_t>(extra);
    }
    return true;
}

bool readText(const uint8_t* bytes, size_t end, size_t* cursor,
              size_t limit, std::string* output) {
    uint16_t length = 0;
    if (!readU16(bytes, end, cursor, &length) || length > limit ||
        *cursor > end || end - *cursor < length) return false;
    const std::string_view view(reinterpret_cast<const char*>(bytes + *cursor), length);
    if (!utf8Valid(view)) return false;
    output->assign(view.data(), view.size());
    *cursor += length;
    return true;
}

bool validKey(std::string_view key) {
    if (key.empty()) return false;
    for (char c : key)
        if (c == '\0' || c == '\r' || c == '\n' || c == '/' || c == '\\')
            return false;
    return true;
}

bool validSystemKey(std::string_view key) {
    if (key.size() < 38 || key[36] != '/' || !validKey(key.substr(37)))
        return false;
    for (size_t i = 0; i < 36; ++i) {
        const char c = key[i];
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? c != '-' : !((c >= '0' && c <= '9') ||
                                  (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

uint32_t crc32(const uint8_t* bytes, size_t count) {
    uint32_t result = 0xFFFFFFFFu;
    for (size_t i = 0; i < count; ++i) {
        result ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            result = (result >> 1) ^ ((result & 1) ? 0xEDB88320u : 0u);
    }
    return ~result;
}

bool safeWorldPath(const std::string& path, std::string* folder, std::string* basename) {
    if (path.size() < 6 || path.size() > 2048 || path.front() != '/' ||
        path.find('\0') != std::string::npos || !utf8Valid(path))
        return false;
    size_t start = 1;
    while (start < path.size()) {
        const size_t slash = path.find('/', start);
        const std::string_view segment(path.data() + start,
            (slash == std::string::npos ? path.size() : slash) - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    const size_t split = path.rfind('/');
    if (split == std::string::npos || split + 1 >= path.size()) return false;
    const std::string filename = path.substr(split + 1);
    if (filename.size() > 170 || filename.size() < 5 ||
        filename.compare(filename.size() - 4, 4, ".wld") != 0) return false;
    *folder = split == 0 ? "/" : path.substr(0, split);
    *basename = filename + kSuffix;
    return basename->size() <= 255;
}

int openFolder(const std::string& folder) {
    return ::open(folder.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}

Result readFile(int folderFd, const std::string& name, std::vector<uint8_t>* output) {
    output->clear();
    int fd = ::openat(folderFd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) return errno == ENOENT ? Result::Missing : Result::IoError;
    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || static_cast<uint64_t>(st.st_size) > kMaxDocument) {
        ::close(fd);
        return Result::Corrupt;
    }
    output->resize(static_cast<size_t>(st.st_size));
    size_t filled = 0;
    while (filled < output->size()) {
        const ssize_t n = ::read(fd, output->data() + filled, output->size() - filled);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ::close(fd); output->clear(); return Result::IoError; }
        filled += static_cast<size_t>(n);
    }
    uint8_t more = 0;
    const ssize_t trailing = ::read(fd, &more, 1);
    const int closed = ::close(fd);
    if (trailing != 0 || closed != 0) { output->clear(); return Result::IoError; }
    if (!NativeWorldStore::validDocument(output->data(), output->size())) {
        output->clear();
        return Result::Corrupt;
    }
    return Result::Ok;
}

bool writeAll(int fd, const std::vector<uint8_t>& bytes) {
    size_t written = 0;
    while (written < bytes.size()) {
        const ssize_t n = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

bool NativeWorldStore::validDocument(const uint8_t* data, size_t size) {
    if (!data || size < 10 || size > kMaxDocument ||
        std::memcmp(data, "BWC1", 4) != 0) return false;
    const size_t end = size - 4;
    const uint32_t saved = static_cast<uint32_t>(data[end]) |
                           static_cast<uint32_t>(data[end + 1]) << 8 |
                           static_cast<uint32_t>(data[end + 2]) << 16 |
                           static_cast<uint32_t>(data[end + 3]) << 24;
    if (crc32(data, end) != saved) return false;
    size_t at = 4;
    uint16_t count = 0;
    if (!readU16(data, end, &at, &count) || count > kMaxSystems) return false;
    std::string previousSystem;
    for (uint16_t system = 0; system < count; ++system) {
        std::string key;
        uint16_t fieldCount = 0;
        if (!readText(data, end, &at, kMaxName, &key) ||
            !validSystemKey(key) || (!previousSystem.empty() && key <= previousSystem) ||
            !readU16(data, end, &at, &fieldCount) ||
            fieldCount == 0 || fieldCount > kMaxFields) return false;
        previousSystem = key;
        std::string previousField;
        for (uint16_t field = 0; field < fieldCount; ++field) {
            std::string name;
            if (!readText(data, end, &at, kMaxName, &name) ||
                !validKey(name) || (!previousField.empty() && name <= previousField) ||
                at >= end) return false;
            previousField = name;
            const uint8_t kind = data[at++];
            if (kind == 1) {
                if (at >= end || data[at++] > 1) return false;
            } else if (kind == 2) {
                if (end - at < 4) return false;
                at += 4;
            } else if (kind == 3) {
                std::string value;
                if (!readText(data, end, &at, kMaxText, &value)) return false;
            } else return false;
        }
    }
    return at == end;
}

bool NativeWorldStore::beginWorld(uint64_t epoch, const std::string& worldPath,
                                 bool cloudSave) {
    std::lock_guard<std::mutex> guard(mutex_);
    active_ = false;
    folder_.clear();
    basename_.clear();
    epoch_ = epoch;
    if (epoch == 0 || cloudSave ||
        !safeWorldPath(worldPath, &folder_, &basename_)) return false;
    const int dir = openFolder(folder_);
    if (dir < 0) return false;
    struct stat st {};
    const bool good = ::fstat(dir, &st) == 0 && S_ISDIR(st.st_mode);
    ::close(dir);
    active_ = good;
    return good;
}

void NativeWorldStore::endWorld(uint64_t epoch) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (epoch_ != epoch) return;
    active_ = false;
    folder_.clear();
    basename_.clear();
}

bool NativeWorldStore::sessionValid(uint64_t epoch) const {
    return active_ && epoch != 0 && epoch == epoch_;
}

bool NativeWorldStore::active(uint64_t epoch) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return sessionValid(epoch);
}

Result NativeWorldStore::read(uint64_t epoch, std::vector<uint8_t>* document) {
    if (!document) return Result::IoError;
    document->clear();
    std::lock_guard<std::mutex> guard(mutex_);
    if (!sessionValid(epoch)) return Result::InvalidSession;
    const int dir = openFolder(folder_);
    if (dir < 0) return Result::IoError;
    const Result result = readFile(dir, basename_, document);
    ::close(dir);
    return result;
}

Result NativeWorldStore::write(uint64_t epoch, const std::vector<uint8_t>& document) {
    if (!validDocument(document.data(), document.size())) return Result::Corrupt;
    std::lock_guard<std::mutex> guard(mutex_);
    if (!sessionValid(epoch)) return Result::InvalidSession;
    const int dir = openFolder(folder_);
    if (dir < 0) return Result::IoError;

    // Impede sobrescrever um save previamente corrompido ou um symlink.
    std::vector<uint8_t> old;
    const Result present = readFile(dir, basename_, &old);
    if (present != Result::Missing && present != Result::Ok) {
        ::close(dir);
        return present;
    }

    std::string temporary;
    int fd = -1;
    for (int tries = 0; tries < 8 && fd < 0; ++tries) {
        const uint64_t ticket = g_tempCounter.fetch_add(1, std::memory_order_relaxed);
        temporary = basename_ + ".tmp." + std::to_string(::getpid()) + "." +
                    std::to_string(ticket);
        fd = ::openat(dir, temporary.c_str(),
                      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) { ::close(dir); return Result::IoError; }

    bool good = writeAll(fd, document) && ::fsync(fd) == 0;
    if (::close(fd) != 0) good = false;
    if (good) {
        // Rename no mesmo diretório após fsync do conteúdo. Não toca no .wld.
        good = ::renameat(dir, temporary.c_str(), dir, basename_.c_str()) == 0;
        if (good && ::fsync(dir) != 0) {
            // O rename já ocorreu. Nunca retornar sucesso sem durabilidade.
            ::close(dir);
            return Result::IoError;
        }
    }
    if (!good) ::unlinkat(dir, temporary.c_str(), 0);
    ::close(dir);
    return good ? Result::Ok : Result::IoError;
}

} // namespace bl::csharp::worldsave
