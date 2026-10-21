#include "experiments/csharp/NativeWorldNetwork.h"
#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::worldsave {
namespace {
uint16_t read16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
        static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8);
}
uint32_t read32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
        static_cast<uint32_t>(data[1]) << 8 |
        static_cast<uint32_t>(data[2]) << 16 |
        static_cast<uint32_t>(data[3]) << 24;
}
uint32_t crc32(const uint8_t* bytes, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (crc & 1u ? 0xEDB88320u : 0u);
    }
    return ~crc;
}
bool validKey(const uint8_t* data, size_t length) {
    if (length < 39 || length > 240) return false;
    if (data[36] != '/') return false;
    for (size_t i = 0; i < 36; ++i) {
        const char c = static_cast<char>(data[i]);
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else if (!((c >= '0' && c <= '9') ||
                     (c >= 'a' && c <= 'f'))) return false;
    }
    for (size_t i = 37; i < length; ++i) {
        const char c = static_cast<char>(data[i]);
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_'))
            return false;
    }
    return true;
}
bool askMono(MonoWorldRequest request, MonoWorldReply* reply, std::string* error) {
    if (!error || !reply || request.epoch == 0 ||
        request.epoch > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return false;
    if (!requestManagedWorldOnMono(request, reply, 3000)) {
        *error = "MonoVM network request timed out or was cancelled";
        return false;
    }
    if (!reply->success) {
        *error = reply->error.empty() ? "MonoVM network action rejected" : reply->error;
        return false;
    }
    return true;
}
}

bool validWorldNetworkPacket(const uint8_t* bytes, size_t length) {
    if (!bytes || length < 14 || length > kWorldNetworkMaxBytes ||
        bytes[0] != 'B' || bytes[1] != 'W' || bytes[2] != 'N' || bytes[3] != '1')
        return false;
    const size_t payloadEnd = length - sizeof(uint32_t);
    if (read32(bytes + payloadEnd) != crc32(bytes, payloadEnd) ||
        read32(bytes + 4) == 0) return false;
    const uint16_t count = read16(bytes + 8);
    if (count > 512) return false;
    size_t cursor = 10;
    std::string previous;
    for (uint16_t i = 0; i < count; ++i) {
        if (payloadEnd - cursor < 2) return false;
        const size_t keyLength = read16(bytes + cursor);
        cursor += 2;
        if (keyLength > payloadEnd - cursor ||
            !validKey(bytes + cursor, keyLength)) return false;
        std::string key(reinterpret_cast<const char*>(bytes + cursor), keyLength);
        if (!previous.empty() && key <= previous) return false;
        previous = std::move(key);
        cursor += keyLength;
        if (payloadEnd - cursor < 2) return false;
        const size_t dataLength = read16(bytes + cursor);
        cursor += 2;
        if (dataLength > 8192 || dataLength > payloadEnd - cursor) return false;
        cursor += dataLength;
    }
    return cursor == payloadEnd;
}

bool prepareWorldNetworkPacket(uint64_t epoch, std::vector<uint8_t>* packet,
                               std::string* error) {
    if (!packet || !error) return false;
    packet->clear();
    error->clear();
    MonoWorldRequest request{};
    request.action = MonoWorldAction::NetSend;
    request.epoch = epoch;
    MonoWorldReply reply{};
    if (!askMono(std::move(request), &reply, error)) return false;
    if (reply.payload.empty() ||
        reply.payload.size() > MonoWorldMailbox::MaxNetPayload ||
        !decodeWorldBase64(reply.payload, packet) ||
        !validWorldNetworkPacket(packet->data(), packet->size())) {
        packet->clear();
        *error = "Invalid outgoing BWN1 world state";
        return false;
    }
    return true;
}

bool receiveWorldNetworkPacket(uint64_t epoch, const uint8_t* bytes, size_t length,
                               std::string* error) {
    if (!error) return false;
    error->clear();
    if (!validWorldNetworkPacket(bytes, length)) {
        *error = "Invalid incoming BWN1 world packet";
        return false;
    }
    const std::vector<uint8_t> copy(bytes, bytes + length);
    MonoWorldRequest request{};
    request.action = MonoWorldAction::NetReceive;
    request.epoch = epoch;
    if (!encodeWorldBase64(copy, &request.payload) ||
        request.payload.size() > MonoWorldMailbox::MaxNetPayload) {
        *error = "BWN1 transport encoding exceeded bounds";
        return false;
    }
    MonoWorldReply reply{};
    return askMono(std::move(request), &reply, error);
}
}
