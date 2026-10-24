#pragma once

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>

#include <unistd.h>

namespace bl::csharp::world_save {

using Key = std::pair<std::string, std::string>;
using Records = std::map<Key, std::string>;

inline bool validName(const std::string& s, size_t max) {
    if (s.empty() || s.size() > max) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

inline bool validPayload(const std::string& s) {
    if (s.empty() || s.size() > 16384 ||
        s.find('\0') != std::string::npos) return false;
    for (unsigned char c : s)
        if ((c < 32 && c != '\n' && c != '\t') || c > 126) return false;
    return s.rfind("bunny-world-data 1\n", 0) == 0;
}

inline char hexDigit(unsigned n) {
    return "0123456789ABCDEF"[n & 15];
}

inline std::string hexEncode(const std::string& data) {
    std::string output;
    output.reserve(data.size() * 2);
    for (unsigned char c : data) {
        output.push_back(hexDigit(c >> 4));
        output.push_back(hexDigit(c));
    }
    return output;
}

inline int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline bool hexDecode(const std::string& input, std::string* output) {
    if (!output || input.empty() || input.size() > 32768 ||
        (input.size() & 1)) return false;
    output->clear();
    output->reserve(input.size() / 2);
    for (size_t i = 0; i < input.size(); i += 2) {
        int hi = hexValue(input[i]), lo = hexValue(input[i + 1]);
        if (hi < 0 || lo < 0) return false;
        output->push_back(static_cast<char>((hi << 4) | lo));
    }
    return validPayload(*output);
}

inline bool readFile(const std::string& path, Records* records) {
    if (!records) return false;
    records->clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::string line;
    if (!std::getline(file, line) || line != "bunny-world-systems 1") return false;
    size_t total = line.size() + 1;
    while (std::getline(file, line)) {
        total += line.size() + 1;
        if (total > 2097152 || line.size() > 33000 || records->size() >= 128)
            return false;
        const size_t a = line.find('\t');
        const size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
        if (b == std::string::npos || line.find('\t', b + 1) != std::string::npos)
            return false;
        std::string mod = line.substr(0, a), system = line.substr(a + 1, b - a - 1);
        std::string payload;
        if (!validName(mod, 128) || !validName(system, 128) ||
            !hexDecode(line.substr(b + 1), &payload) ||
            !records->emplace(Key{std::move(mod), std::move(system)},
                              std::move(payload)).second) return false;
    }
    return file.eof();
}

inline bool writeFile(const std::string& path, const Records& records) {
    if (path.empty() || path.size() > 2100 || records.size() > 128)
        return false;
    const std::string tmp = path + ".tmp";
    FILE* file = std::fopen(tmp.c_str(), "wb");
    if (!file) return false;
    bool ok = std::fputs("bunny-world-systems 1\n", file) >= 0;
    size_t total = 22;
    for (const auto& [key, data] : records) {
        if (!ok || !validName(key.first, 128) ||
            !validName(key.second, 128) || !validPayload(data)) {
            ok = false;
            break;
        }
        std::string hex = hexEncode(data);
        total += key.first.size() + key.second.size() + hex.size() + 3;
        if (total > 2097152 ||
            std::fprintf(file, "%s\t%s\t%s\n", key.first.c_str(),
                         key.second.c_str(), hex.c_str()) < 0) {
            ok = false;
            break;
        }
    }
    if (ok) ok = std::fflush(file) == 0;
    if (ok) ok = fsync(fileno(file)) == 0;
    if (std::fclose(file) != 0) ok = false;
    if (ok) ok = std::rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok) std::remove(tmp.c_str());
    return ok;
}

inline int readState(const std::string& path, const std::string& mod,
                     const std::string& system, std::string* payload) {
    if (!payload || !validName(mod, 128) || !validName(system, 128))
        return -1;
    std::ifstream file(path, std::ios::binary);
    if (!file) return 0;
    file.close();
    Records records;
    if (!readFile(path, &records)) return -1;
    auto found = records.find({mod, system});
    if (found == records.end()) return 0;
    *payload = found->second;
    return 1;
}

inline bool writeState(const std::string& path, const std::string& mod,
                       const std::string& system, const std::string& payload) {
    if (!validName(mod, 128) || !validName(system, 128) ||
        !validPayload(payload)) return false;
    Records records;
    std::ifstream file(path, std::ios::binary);
    if (file) {
        file.close();
        if (!readFile(path, &records)) return false;
    }
    auto key = Key{mod, system};
    auto it = records.find(key);
    if (it != records.end() && it->second == payload) return true;
    records[key] = payload;
    return writeFile(path, records);
}

}
