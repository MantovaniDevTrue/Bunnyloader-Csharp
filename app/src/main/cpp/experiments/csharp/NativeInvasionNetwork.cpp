#include "experiments/csharp/NativeInvasionNetwork.h"
#include "experiments/csharp/NativeInvasionPeerPolicy.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "menu/NetRequests.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

using Key = std::pair<std::string, std::string>;
struct Packet {
    std::string payload;
    uint64_t revision = 0;
};
struct Peer {
    uint64_t nonce = 0;
    uint64_t lastSeenTick = 0;
    std::map<Key, uint64_t> delivered;
    std::map<Key, uint64_t> worldDelivered;
};
struct Received {
    uint64_t localSession = 0;
    std::string mod;
    std::string payload;
};

std::mutex g_mutex;
std::map<Key, Packet> g_hostStates;
std::map<Key, Packet> g_worldStates;
std::map<int, Peer> g_peers;
std::deque<Received> g_received;
std::deque<Received> g_worldReceived;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};
bool g_dirty = false;
uint64_t g_nonce = 0;
uint32_t g_joinTimer = 0;
uint64_t g_hostTick = 0;
using NewTextFn = void (*)(Il2CppString*, uint8_t, uint8_t, uint8_t, bool,
                           const MethodInfo*);
NewTextFn g_originalText = nullptr;
bool g_textHook = false;

void pruneExpiredPeersLocked() {
    for (auto it = g_peers.begin(); it != g_peers.end();) {
        if (invasion_peer::expired(g_hostTick, it->second.lastSeenTick))
            it = g_peers.erase(it);
        else
            ++it;
    }
}

bool validName(const std::string& value, size_t maximum) {
    if (value.empty() || value.size() > maximum) return false;
    for (char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    return true;
}

bool decimal(const std::string& text, uint64_t* value) {
    if (text.empty() || text.size() > 20) return false;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    auto result = std::from_chars(begin, end, *value);
    return result.ec == std::errc() && result.ptr == end;
}

std::vector<std::string> fields(const std::string& text, char separator) {
    std::vector<std::string> values;
    size_t from = 0;
    while (from <= text.size() && values.size() < 16) {
        size_t end = text.find(separator, from);
        if (end == std::string::npos) end = text.size();
        values.emplace_back(text, from, end - from);
        from = end + 1;
    }
    return values;
}

bool parseSnapshot(const std::string& text, Key* id, uint64_t* revision) {
    if (text.size() > 300 || text.rfind("INV1|", 0) != 0) return false;
    const auto components = fields(text, '|');
    if (components.size() != 11 || !validName(components[2], 128) ||
        !validName(components[3], 64)) return false;
    uint64_t world = 0, epoch = 0, serial = 0, phase = 0, progress = 0;
    uint64_t goal = 0, active = 0, pending = 0;
    if (!decimal(components[1], &world) || world == 0 ||
        !decimal(components[4], &epoch) || epoch == 0 ||
        !decimal(components[5], &serial) ||
        !decimal(components[6], &phase) ||
        !decimal(components[7], &progress) ||
        !decimal(components[8], &goal) ||
        !decimal(components[9], &active) ||
        !decimal(components[10], &pending) ||
        phase > 2 || goal == 0 || goal > 1000000 ||
        progress > goal || active > 64 || pending > 64 ||
        active + pending > 64 || (phase == 2 && progress != goal) ||
        (phase == 1 && progress == goal) ||
        (phase == 0 && (progress != 0 || active != 0 || pending != 0)))
        return false;
    *id = {components[2], components[3]};
    *revision = (epoch << 32) ^ serial;
    return true;
}

bool parseWorldSnapshot(const std::string& text, Key* id, uint64_t* revision) {
    if (text.size() > 300 || text.rfind("WS1|", 0) != 0) return false;
    const auto parts = fields(text, '|');
    if (parts.size() != 6 || !validName(parts[2], 128) ||
        !validName(parts[3], 128)) return false;
    uint64_t session = 0, sequence = 0;
    if (!decimal(parts[1], &session) || session == 0 ||
        !decimal(parts[4], &sequence) || sequence == 0 ||
        parts[5].empty() || parts[5].size() > 192 ||
        parts[5].size() % 2 != 0) return false;
    for (char c : parts[5])
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
            return false;
    *id = {parts[2], parts[3]};
    *revision = sequence;
    return true;
}

std::string utf8(Il2CppString* value) {
    if (!value || value->length < 0 || value->length > 350) return {};
    std::string result;
    result.reserve(static_cast<size_t>(value->length));
    for (int i = 0; i < value->length; ++i) {
        const char16_t c = value->chars[i];
        if (c > 127) return {};
        result.push_back(static_cast<char>(c));
    }
    return result;
}

void onText(Il2CppString* message, uint8_t red, uint8_t green, uint8_t blue,
            bool forced, const MethodInfo* method) {
    if (runtime::netMode() == 1 && message && g_ready.load(std::memory_order_acquire)) {
        const std::string text = utf8(message);
        if (text.rfind("BLINV2|", 0) == 0) {
            acceptNativeInvasionChat(text);
            return;
        }
        if (text.rfind("BLWS2|", 0) == 0) {
            acceptNativeWorldSystemChat(text);
            return;
        }
    }
    if (g_originalText) g_originalText(message, red, green, blue, forced, method);
}

bool installClientChatHook() {
    if (g_textHook) return true;
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* method = main ? il2cpp::api().class_get_method_from_name(main, "NewText", 5) : nullptr;
    if (!method || !hook::install(method, onText, &g_originalText)) return false;
    g_textHook = true;
    return true;
}

}

void setNativeInvasionNetWorldState(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    const bool reset = g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready);
    if (reset) {
        g_hostStates.clear();
        g_worldStates.clear();
        g_peers.clear();
        g_received.clear();
        g_worldReceived.clear();
        g_dirty = false;
        g_nonce = 0;
        g_joinTimer = 0;
        g_hostTick = 0;
    }
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session != 0, std::memory_order_release);
}

bool registerNativeInvasionClient(int client, uint64_t nonce) {
    if (!runtime::isNetHost() || client < 0 || client >= 255 || nonce == 0 ||
        !g_ready.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    pruneExpiredPeersLocked();
    if (g_peers.size() >= 32 && !g_peers.contains(client)) return false;
    Peer& peer = g_peers[client];
    peer.lastSeenTick = g_hostTick;
    if (peer.nonce != nonce) {
        peer.nonce = nonce;
        peer.delivered.clear();
        peer.worldDelivered.clear();
        g_dirty = true;
    }
    return true;
}

bool acceptNativeInvasionChat(const std::string& text) {
    if (runtime::netMode() != 1 || text.size() > 350 ||
        text.rfind("BLINV2|", 0) != 0) return false;
    size_t second = text.find('|', 7);
    if (second == std::string::npos) return false;
    uint64_t token = 0;
    if (!decimal(text.substr(7, second - 7), &token)) return false;
    Key id;
    uint64_t revision = 0;
    const std::string encoded = text.substr(second + 1);
    if (!parseSnapshot(encoded, &id, &revision)) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) || g_nonce == 0 ||
        token != g_nonce || g_received.size() >= 64) return false;
    g_received.push_back({g_session.load(std::memory_order_relaxed), id.first, encoded});
    return true;
}

bool acceptNativeWorldSystemChat(const std::string& text) {
    if (runtime::netMode() != 1 || text.size() > 350 ||
        text.rfind("BLWS2|", 0) != 0) return false;
    size_t second = text.find('|', 6);
    if (second == std::string::npos) return false;
    uint64_t token = 0;
    if (!decimal(text.substr(6, second - 6), &token)) return false;
    Key id;
    uint64_t revision = 0;
    const std::string encoded = text.substr(second + 1);
    if (!parseWorldSnapshot(encoded, &id, &revision)) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) || g_nonce == 0 ||
        token != g_nonce || g_worldReceived.size() >= 64) return false;
    g_worldReceived.push_back({g_session.load(std::memory_order_relaxed),
                               id.first, encoded});
    return true;
}

void pumpNativeInvasionNetworkOnGameThread() {
    if (!g_ready.load(std::memory_order_acquire)) return;
    if (runtime::netMode() == 1) {
        installClientChatHook();
        if (!g_textHook) return;
        uint64_t token = 0;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (g_nonce == 0) {
                uint64_t next = 0;
                // Bionic exposes arc4random_buf since API 21; getrandom starts
                // at API 28 and cannot be linked by our minimum-API-24 APK.
                arc4random_buf(&next, sizeof(next));
                if (next == 0) return;
                g_nonce = next;
            }
            if (++g_joinTimer < 300) return;
            g_joinTimer = 0;
            token = g_nonce;
        }
        runtime::sendToServer("invjoin " + std::to_string(token));
        return;
    }
    if (!runtime::isNetHost()) return;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        ++g_hostTick;
        if (g_hostTick % invasion_peer::sweepInterval == 0)
            pruneExpiredPeersLocked();
    }
    for (int sent = 0; sent < 2; ++sent) {
        int player = -1;
        uint64_t nonce = 0, revision = 0;
        Key id;
        std::string packet;
        bool worldPacket = false;
        {
            std::lock_guard<std::mutex> guard(g_mutex);
            if (!g_dirty) return;
            g_dirty = false;
            for (auto& [client, peer] : g_peers) {
                for (const auto& [key, state] : g_hostStates) {
                    if (peer.delivered.contains(key) &&
                        peer.delivered[key] == state.revision) continue;
                    player = client;
                    nonce = peer.nonce;
                    revision = state.revision;
                    id = key;
                    packet = state.payload;
                    g_dirty = true;
                    break;
                }
                if (player >= 0) break;
                for (const auto& [key, state] : g_worldStates) {
                    if (peer.worldDelivered.contains(key) &&
                        peer.worldDelivered[key] == state.revision) continue;
                    player = client;
                    nonce = peer.nonce;
                    revision = state.revision;
                    id = key;
                    packet = state.payload;
                    worldPacket = true;
                    g_dirty = true;
                    break;
                }
                if (player >= 0) break;
            }
        }
        if (player < 0) return;
        const bool ok = runtime::sendNetworkChat(
            std::string(worldPacket ? "BLWS2|" : "BLINV2|") +
                std::to_string(nonce) + "|" + packet, player);
        std::lock_guard<std::mutex> guard(g_mutex);
        if (ok && g_peers.contains(player) && g_peers[player].nonce == nonce) {
            if (worldPacket) g_peers[player].worldDelivered[id] = revision;
            else g_peers[player].delivered[id] = revision;
        }
        if (!ok) return;
    }
}

}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_net_publish(int64_t session, const char* payload) {
    using namespace bl::csharp;
    if (!payload || session <= 0 || strnlen(payload, 301) > 300) return 0;
    Key id;
    uint64_t revision = 0;
    const std::string encoded(payload);
    if (!parseSnapshot(encoded, &id, &revision)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    if (g_hostStates.size() >= 64 && !g_hostStates.contains(id)) return 0;
    auto& packet = g_hostStates[id];
    if (packet.payload != encoded) {
        packet = {encoded, revision};
        g_dirty = true;
    }
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_net_poll(int64_t session, const char* mod,
                            char* buffer, int capacity) {
    using namespace bl::csharp;
    if (session <= 0 || !mod || !buffer || capacity < 301 ||
        !validName(mod, 128)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (bl::runtime::netMode() != 1 || !g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    for (auto it = g_received.begin(); it != g_received.end(); ++it) {
        if (it->localSession != static_cast<uint64_t>(session) ||
            it->mod != mod) continue;
        const int length = static_cast<int>(it->payload.size());
        if (length + 1 > capacity) return 0;
        std::memcpy(buffer, it->payload.data(), length);
        buffer[length] = 0;
        g_received.erase(it);
        return length;
    }
    return 0;
}

extern "C" __attribute__((visibility("default")))
int bunny_world_system_net_publish(int64_t session, const char* payload) {
    using namespace bl::csharp;
    if (!payload || session <= 0 || strnlen(payload, 301) > 300 ||
        !runtime::isNetHost()) return 0;
    Key id;
    uint64_t revision = 0;
    const std::string encoded(payload);
    if (!parseWorldSnapshot(encoded, &id, &revision)) return 0;
    const auto parts = fields(encoded, '|');
    uint64_t remoteSession = 0;
    if (!decimal(parts[1], &remoteSession) ||
        remoteSession != static_cast<uint64_t>(session)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    if (g_worldStates.size() >= 64 && !g_worldStates.contains(id)) return 0;
    auto& packet = g_worldStates[id];
    if (packet.payload != encoded) {
        if (packet.revision > revision) return 0;
        if (packet.revision == revision && !packet.payload.empty()) return 0;
        packet = {encoded, revision};
        g_dirty = true;
    }
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_world_system_net_poll(int64_t session, const char* mod,
                                 char* buffer, int capacity) {
    using namespace bl::csharp;
    if (session <= 0 || !mod || !buffer || capacity < 301 ||
        !validName(mod, 128)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (runtime::netMode() != 1 || !g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    for (auto it = g_worldReceived.begin(); it != g_worldReceived.end(); ++it) {
        if (it->localSession != static_cast<uint64_t>(session) ||
            it->mod != mod) continue;
        const int size = static_cast<int>(it->payload.size());
        if (size + 1 > capacity) return 0;
        std::memcpy(buffer, it->payload.data(), size);
        buffer[size] = 0;
        g_worldReceived.erase(it);
        return size;
    }
    return 0;
}
