#include "experiments/csharp/NativeTouchLayoutJson.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bl::csharp::touch {
namespace {
constexpr size_t kMaxFile = 1024 * 1024;
constexpr size_t kMaxProfiles = 64, kMaxSlots = 256, kMaxGroup = 16;
const char* kFilename = "BunnyLoader.touchbuttons.csharp.json";

bool validText(std::string_view s, size_t limit) {
    if (s.empty() || s.size() > limit) return false;
    for (unsigned char c : s) if (c < 32 || c == 127) return false;
    return true;
}
bool validAction(std::string_view text) {
    auto slash = text.find('/');
    if (slash == std::string_view::npos || slash < 1 ||
        slash >= text.size() - 1 || text.find('/', slash + 1) != text.npos)
        return false;
    const auto validPart = [](std::string_view s) {
        if (s.empty() || s.size() > 80 || s == "." || s == "..") return false;
        for (unsigned char c : s)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
                return false;
        return true;
    };
    return validPart(text.substr(0, slash)) &&
           validPart(text.substr(slash + 1));
}

struct Reader {
    std::string_view s;
    size_t pos = 0;
    void spaces() {
        while (pos < s.size() &&
               (s[pos] == ' ' || s[pos] == '\n' ||
                s[pos] == '\t' || s[pos] == '\r')) ++pos;
    }
    bool take(char c) {
        spaces();
        if (pos >= s.size() || s[pos] != c) return false;
        ++pos;
        return true;
    }
    bool string(std::string* out, size_t limit = 128) {
        if (!take('"')) return false;
        out->clear();
        while (pos < s.size()) {
            unsigned char c = s[pos++];
            if (c == '"') return out->size() <= limit;
            if (c < 32) return false;
            if (c == '\\') {
                if (pos == s.size()) return false;
                c = s[pos++];
                switch(c) {
                    case '"': case '\\': case '/': break;
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'u': {
                        if (s.size() - pos < 4) return false;
                        unsigned value = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = s[pos++];
                            value <<= 4;
                            if (h >= '0' && h <= '9') value += h - '0';
                            else if (h >= 'a' && h <= 'f') value += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') value += h - 'A' + 10;
                            else return false;
                        }
                        if (!value || value >= 128 || value < 32) return false;
                        c = static_cast<unsigned char>(value);
                        break;
                    }
                    default: return false;
                }
            }
            if (out->size() == limit) return false;
            out->push_back(static_cast<char>(c));
        }
        return false;
    }
    bool literal(std::string_view word) {
        spaces();
        if (s.substr(pos, word.size()) != word) return false;
        pos += word.size();
        return true;
    }
    bool integer(int* out) {
        spaces();
        if (pos == s.size() || s[pos] < '0' || s[pos] > '9') return false;
        unsigned value = 0;
        do {
            value = value * 10 + static_cast<unsigned>(s[pos++] - '0');
            if (value > 65536) return false;
        } while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9');
        *out = static_cast<int>(value);
        return true;
    }
    bool nullable(std::string* out, size_t limit = 168) {
        spaces();
        if (literal("null")) {
            out->clear();
            return true;
        }
        if (!string(out, limit)) return false;
        return validAction(*out);
    }
    bool group(std::vector<std::string>* values) {
        if (!take('[')) return false;
        if (take(']')) return true;
        do {
            if (values->size() == kMaxGroup) return false;
            std::string value;
            if (!nullable(&value)) return false;
            values->push_back(value);
            if (take(']')) return true;
        } while (take(','));
        return false;
    }
    bool row(SavedSlot* row) {
        if (!take('{')) return false;
        bool hasSlot = false, hasIndex = false;
        bool seenAction = false, seenGroup = false;
        if (take('}')) return false;
        do {
            std::string key;
            if (!string(&key, 40) || !take(':')) return false;
            if (key == "slot" && !hasSlot) {
                if (!string(&row->slot) || !validText(row->slot, 128)) return false;
                hasSlot = true;
            } else if (key == "index" && !hasIndex) {
                if (!integer(&row->index) || row->index >= 256) return false;
                hasIndex = true;
            } else if (key == "action" && !seenAction) {
                if (!nullable(&row->action)) return false;
                seenAction = true;
            } else if (key == "group" && !seenGroup) {
                if (!group(&row->group)) return false;
                seenGroup = true;
            } else return false;
            if (take('}')) return hasSlot && hasIndex;
        } while (take(','));
        return false;
    }
    bool rows(std::vector<SavedSlot>* values) {
        if (!take('[')) return false;
        if (take(']')) return true;
        std::set<std::string> unique;
        do {
            if (values->size() == kMaxSlots) return false;
            SavedSlot row;
            if (!this->row(&row) || !unique.insert(row.slot).second) return false;
            values->push_back(std::move(row));
            if (take(']')) return true;
        } while (take(','));
        return false;
    }
    bool profiles(SavedProfiles* data) {
        if (!take('{')) return false;
        if (take('}')) return true;
        do {
            if (data->size() >= kMaxProfiles) return false;
            std::string key;
            if (!string(&key, 100) || !validText(key, 100) ||
                !take(':')) return false;
            std::vector<SavedSlot> items;
            if (!rows(&items) || !data->emplace(key, std::move(items)).second)
                return false;
            if (take('}')) return true;
        } while (take(','));
        return false;
    }
    bool root(SavedProfiles* data) {
        if (!take('{')) return false;
        bool version = false, profilesFound = false;
        do {
            std::string key;
            if (!string(&key, 40) || !take(':')) return false;
            if (key == "version" && !version) {
                int number = 0;
                if (!integer(&number) || number != 1) return false;
                version = true;
            } else if (key == "profiles" && !profilesFound) {
                if (!profiles(data)) return false;
                profilesFound = true;
            } else return false;
            if (take('}')) {
                spaces();
                return version && profilesFound && pos == s.size();
            }
        } while (take(','));
        return false;
    }
};

void string(std::string* out, std::string_view value) {
    out->push_back('"');
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out->push_back('\\');
        out->push_back(static_cast<char>(c));
    }
    out->push_back('"');
}
void nullable(std::string* out, std::string_view value) {
    if (value.empty()) *out += "null";
    else string(out, value);
}
bool pathFor(const std::string& dir, std::string* output) {
    if (dir.empty() || dir.size() > 2048 ||
        dir.find("..") != std::string::npos ||
        dir.find('\0') != std::string::npos) return false;
    *output = dir + (dir.back() == '/' ? "" : "/") + kFilename;
    return true;
}
}

bool readTouchLayoutJson(std::string_view json, SavedProfiles* data) {
    if (!data || json.size() > kMaxFile) return false;
    SavedProfiles temp;
    Reader parser{json};
    if (!parser.root(&temp)) return false;
    *data = std::move(temp);
    return true;
}
std::string writeTouchLayoutJson(const SavedProfiles& data) {
    if (data.size() > kMaxProfiles) return {};
    std::string result = "{\"version\":1,\"profiles\":{";
    bool firstProfile = true;
    for (const auto& [profile, rows] : data) {
        if (!validText(profile, 100) || rows.size() > kMaxSlots) return {};
        if (!firstProfile) result.push_back(',');
        firstProfile = false;
        string(&result, profile);
        result += ":[";
        bool firstRow = true;
        std::set<std::string> keys;
        for (const auto& row : rows) {
            if (!validText(row.slot, 128) || row.index < 0 ||
                row.index >= 256 || row.group.size() > kMaxGroup ||
                !keys.insert(row.slot).second ||
                (!row.action.empty() && !validAction(row.action)))
                return {};
            if (!firstRow) result.push_back(',');
            firstRow = false;
            result += "{\"slot\":";
            string(&result, row.slot);
            result += ",\"index\":";
            result += std::to_string(row.index);
            result += ",\"action\":";
            nullable(&result, row.action);
            result += ",\"group\":[";
            for (size_t i = 0; i < row.group.size(); ++i) {
                if (!row.group[i].empty() && !validAction(row.group[i])) return {};
                if (i) result.push_back(',');
                nullable(&result, row.group[i]);
            }
            result += "]}";
            if (result.size() > kMaxFile) return {};
        }
        result += "]";
    }
    result += "}}";
    return result.size() <= kMaxFile ? result : std::string{};
}
bool loadTouchLayoutFile(const std::string& dir, SavedProfiles* data) {
    std::string path;
    if (!data || !pathFor(dir, &path)) return false;
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0) {
        if (errno != ENOENT) return false;
        data->clear();
        return true;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 1 ||
        static_cast<uint64_t>(st.st_size) > kMaxFile) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::string json((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    return json.size() == static_cast<size_t>(st.st_size) &&
           readTouchLayoutJson(json, data);
}
bool saveTouchLayoutFile(const std::string& dir, const SavedProfiles& data) {
    std::string path;
    if (!pathFor(dir, &path)) return false;
    const std::string json = writeTouchLayoutJson(data);
    if (json.empty()) return false;
    struct stat current{};
    if (lstat(path.c_str(), &current) == 0) {
        if (!S_ISREG(current.st_mode)) return false;
    } else if (errno != ENOENT) {
        return false;
    }
    const std::string temp = path + ".tmp";
    const int fd = open(temp.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                        0600);
    if (fd < 0) return false;
    FILE* output = fdopen(fd, "wb");
    if (!output) {
        close(fd);
        unlink(temp.c_str());
        return false;
    }
    bool ok = fwrite(json.data(), 1, json.size(), output) == json.size();
    if (fflush(output) != 0) ok = false;
    if (fsync(fileno(output)) != 0) ok = false;
    if (fclose(output) != 0) ok = false;
    if (!ok || rename(temp.c_str(), path.c_str()) != 0) {
        unlink(temp.c_str());
        return false;
    }
    return true;
}
} // namespace bl::csharp::touch
