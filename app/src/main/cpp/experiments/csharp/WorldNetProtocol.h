#pragma once

#include <charconv>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace bl::csharp::world_net {

constexpr uint64_t retryIntervalTicks = 600;

inline bool worldStateDue(uint64_t now, uint64_t lastSent,
                          uint64_t deliveredRevision, uint64_t currentRevision) {
    if (deliveredRevision != currentRevision || lastSent == 0) return true;
    return now >= lastSent && now - lastSent >= retryIntervalTicks;
}

inline bool preferWorldForSendSlot(int sent) {
    return (sent & 1) != 0;
}

template <typename PeerMap>
typename PeerMap::iterator nextPeerAfter(PeerMap& peers, int last) {
    auto it = peers.upper_bound(last);
    return it == peers.end() ? peers.begin() : it;
}

struct Snapshot {
    uint64_t session = 0;
    uint64_t revision = 0;
    std::string mod;
    std::string system;
};

inline bool name(std::string_view value, size_t max) {
    if (value.empty() || value.size() > max) return false;
    for (char ch : value)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) return false;
    return true;
}

inline bool decimal(std::string_view value, uint64_t* result) {
    if (!result || value.empty() || value.size() > 20) return false;
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), *result);
    return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size();
}

struct Queued {
    uint64_t localSession = 0;
    uint64_t hostSession = 0;
    uint64_t revision = 0;
    std::string mod;
    std::string system;
    std::string payload;
};

inline bool enqueueLatest(std::deque<Queued>* queue, uint64_t localSession,
                          const Snapshot& snapshot, const std::string& payload,
                          size_t maximum = 64) {
    if (!queue || localSession == 0 || snapshot.session == 0 ||
        snapshot.revision == 0 || payload.empty()) return false;
    for (Queued& current : *queue) {
        if (current.localSession != localSession ||
            current.mod != snapshot.mod || current.system != snapshot.system)
            continue;
        if (current.hostSession != snapshot.session ||
            current.revision >= snapshot.revision) return false;
        current.revision = snapshot.revision;
        current.payload = payload;
        return true;
    }
    if (queue->size() >= maximum) return false;
    queue->push_back({localSession, snapshot.session, snapshot.revision,
                      snapshot.mod, snapshot.system, payload});
    return true;
}

inline bool parse(std::string_view data, Snapshot* result) {
    if (!result || data.size() > 300 || data.substr(0, 4) != "WS1|") return false;
    std::string_view parts[6];
    size_t begin = 0;
    for (size_t i = 0; i < 6; i++) {
        size_t end = data.find('|', begin);
        if (i == 5) {
            if (end != std::string_view::npos) return false;
            end = data.size();
        } else if (end == std::string_view::npos) return false;
        parts[i] = data.substr(begin, end - begin);
        begin = end + 1;
    }
    uint64_t session = 0, revision = 0;
    if (!decimal(parts[1], &session) || session == 0 ||
        !decimal(parts[4], &revision) || revision == 0 ||
        !name(parts[2], 128) || !name(parts[3], 128) ||
        parts[5].size() < 2 || parts[5].size() > 192 ||
        parts[5].size() % 2 != 0) return false;
    for (char ch : parts[5])
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
            return false;
    result->session = session;
    result->revision = revision;
    result->mod.assign(parts[2]);
    result->system.assign(parts[3]);
    return true;
}

}
