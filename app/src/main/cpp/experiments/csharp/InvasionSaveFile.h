#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <utility>

#include <unistd.h>

namespace bl::csharp::invasion_save {

struct State {
    int phase = 0;
    int progress = 0;
    int cooldown = 0;
};

inline bool validName(const std::string& name, size_t limit) {
    if (name.empty() || name.size() > limit) return false;
    for (char ch : name)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    return true;
}

inline bool validState(const State& state) {
    return state.phase >= 0 && state.phase <= 2 &&
        state.progress >= 0 && state.progress <= 1000000 &&
        state.cooldown >= 0 && state.cooldown <= 36000 &&
        (state.phase != 0 || state.progress == 0);
}

using Records = std::map<std::pair<std::string, std::string>, State>;

inline bool parseInt(const std::string& value, int* result) {
    if (value.empty() || value.size() > 10) return false;
    long long parsed = 0;
    for (char ch : value) {
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + (ch - '0');
        if (parsed > 1000000) return false;
    }
    *result = static_cast<int>(parsed);
    return true;
}

inline bool readFile(const std::string& path, Records* records) {
    if (!records) return false;
    records->clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    std::string line;
    if (!std::getline(file, line) || line != "bunny-invasions 1") return false;
    size_t total = line.size() + 1;
    while (std::getline(file, line)) {
        total += line.size() + 1;
        if (total > 131072 || line.size() > 512 || records->size() >= 512)
            return false;
        const size_t a = line.find('\t');
        const size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
        const size_t c = b == std::string::npos ? b : line.find('\t', b + 1);
        const size_t d = c == std::string::npos ? c : line.find('\t', c + 1);
        if (d == std::string::npos || line.find('\t', d + 1) != std::string::npos)
            return false;
        auto mod = line.substr(0, a);
        auto id = line.substr(a + 1, b - a - 1);
        State state;
        if (!validName(mod, 128) || !validName(id, 64) ||
            !parseInt(line.substr(b + 1, c - b - 1), &state.phase) ||
            !parseInt(line.substr(c + 1, d - c - 1), &state.progress) ||
            !parseInt(line.substr(d + 1), &state.cooldown) ||
            !validState(state) || !records->emplace(
                std::make_pair(std::move(mod), std::move(id)), state).second)
            return false;
    }
    return file.eof();
}

inline bool saveFile(const std::string& path, const Records& records) {
    if (path.empty() || path.size() > 2100 || records.size() > 512)
        return false;
    const std::string temp = path + ".tmp";
    FILE* file = std::fopen(temp.c_str(), "wb");
    if (!file) return false;
    bool ok = std::fputs("bunny-invasions 1\n", file) >= 0;
    for (const auto& [key, state] : records) {
        if (!validName(key.first, 128) || !validName(key.second, 64) ||
            !validState(state) || !ok) {
            ok = false;
            break;
        }
        if (std::fprintf(file, "%s\t%s\t%d\t%d\t%d\n", key.first.c_str(),
                         key.second.c_str(), state.phase, state.progress,
                         state.cooldown) < 0) ok = false;
    }
    if (ok) ok = std::fflush(file) == 0;
    if (ok) ok = fsync(fileno(file)) == 0;
    if (std::fclose(file) != 0) ok = false;
    if (ok) ok = std::rename(temp.c_str(), path.c_str()) == 0;
    if (!ok) std::remove(temp.c_str());
    return ok;
}

inline bool readState(const std::string& path, const std::string& mod,
                      const std::string& id, State* state) {
    if (!state || !validName(mod, 128) || !validName(id, 64)) return false;
    Records records;
    if (!readFile(path, &records)) return false;
    const auto it = records.find({mod, id});
    if (it == records.end()) return false;
    *state = it->second;
    return true;
}

inline bool writeState(const std::string& path, const std::string& mod,
                       const std::string& id, const State& state) {
    if (!validName(mod, 128) || !validName(id, 64) || !validState(state))
        return false;
    Records records;
    std::ifstream exists(path, std::ios::binary);
    if (exists.good()) {
        exists.close();
        if (!readFile(path, &records)) return false;
    }
    if (state.phase == 0) records.erase({mod, id});
    else records[{mod, id}] = state;
    return saveFile(path, records);
}

}
