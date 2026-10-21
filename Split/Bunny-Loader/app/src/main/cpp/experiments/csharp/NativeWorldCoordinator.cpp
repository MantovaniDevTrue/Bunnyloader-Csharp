#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/NativeWorldStore.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::worldsave {
namespace {
constexpr size_t kMaxBytes = 1024 * 1024;
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool invoke(MonoWorldRequest request, MonoWorldReply* reply, std::string* error) {
    if (!reply || !error || request.epoch > static_cast<uint64_t>(
            std::numeric_limits<int64_t>::max())) return false;
    if (!requestManagedWorldOnMono(request, reply, 3000)) {
        *error = "MonoVM world operation timeout or unavailable";
        return false;
    }
    if (!reply->success) {
        *error = reply->error.empty() ? "MonoVM world callback rejected" : reply->error;
        return false;
    }
    return true;
}
}

bool encodeWorldBase64(const std::vector<uint8_t>& bytes, std::string* encoded) {
    if (!encoded || bytes.size() > kMaxBytes) return false;
    encoded->clear();
    encoded->reserve(4 * ((bytes.size() + 2) / 3));
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t a = bytes[i];
        const uint32_t b = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const uint32_t c = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        const uint32_t word = (a << 16) | (b << 8) | c;
        encoded->push_back(kAlphabet[(word >> 18) & 63]);
        encoded->push_back(kAlphabet[(word >> 12) & 63]);
        encoded->push_back(i + 1 < bytes.size() ? kAlphabet[(word >> 6) & 63] : '=');
        encoded->push_back(i + 2 < bytes.size() ? kAlphabet[word & 63] : '=');
    }
    return true;
}

bool decodeWorldBase64(const std::string& encoded, std::vector<uint8_t>* bytes) {
    if (!bytes) return false;
    bytes->clear();
    if ((encoded.size() & 3) != 0 ||
        encoded.size() > 4 * ((kMaxBytes + 2) / 3)) return false;
    bytes->reserve((encoded.size() / 4) * 3);
    for (size_t i = 0; i < encoded.size(); i += 4) {
        const bool last = i + 4 == encoded.size();
        const int a = digit(encoded[i]);
        const int b = digit(encoded[i + 1]);
        const bool twoPadding = encoded[i + 2] == '=';
        const bool onePadding = encoded[i + 3] == '=';
        if (a < 0 || b < 0 || (twoPadding && !onePadding) ||
            ((onePadding || twoPadding) && !last)) {
            bytes->clear();
            return false;
        }
        const int c = twoPadding ? 0 : digit(encoded[i + 2]);
        const int d = onePadding ? 0 : digit(encoded[i + 3]);
        if (c < 0 || d < 0 || (twoPadding && (b & 15)) ||
            (onePadding && !twoPadding && (c & 3))) {
            bytes->clear();
            return false;
        }
        const uint32_t word = (static_cast<uint32_t>(a) << 18) |
                              (static_cast<uint32_t>(b) << 12) |
                              (static_cast<uint32_t>(c) << 6) |
                               static_cast<uint32_t>(d);
        bytes->push_back(static_cast<uint8_t>(word >> 16));
        if (!twoPadding) bytes->push_back(static_cast<uint8_t>(word >> 8));
        if (!onePadding) bytes->push_back(static_cast<uint8_t>(word));
    }
    if (bytes->size() > kMaxBytes) {
        bytes->clear();
        return false;
    }
    return true;
}

bool enterManagedWorld(uint64_t epoch, bool authoritative, std::string* error) {
    if (!error) return false;
    error->clear();
    if (epoch == 0 || !activeWorldStore().active(epoch)) {
        *error = "World local storage session unavailable";
        return false;
    }
    std::vector<uint8_t> old;
    const Result result = activeWorldStore().read(epoch, &old);
    if (result != Result::Ok && result != Result::Missing) {
        *error = "World sidecar corrupt or unreadable; entry rejected";
        return false;
    }
    MonoWorldRequest enter{};
    enter.action = MonoWorldAction::Enter;
    enter.epoch = epoch;
    enter.authoritative = authoritative;
    if (result == Result::Ok && !encodeWorldBase64(old, &enter.payload)) {
        *error = "World sidecar exceeds transport size";
        return false;
    }
    MonoWorldReply reply{};
    return invoke(std::move(enter), &reply, error);
}

bool saveManagedWorld(uint64_t epoch, bool authoritative, std::string* error) {
    if (!error) return false;
    error->clear();
    if (!authoritative || epoch == 0 || !activeWorldStore().active(epoch)) {
        *error = "World snapshot rejected: client or stale session";
        return false;
    }
    MonoWorldRequest prepare{};
    prepare.action = MonoWorldAction::Prepare;
    prepare.epoch = epoch;
    MonoWorldReply prepared{};
    if (!invoke(std::move(prepare), &prepared, error)) return false;
    std::vector<uint8_t> bytes;
    if (!decodeWorldBase64(prepared.payload, &bytes) ||
        !NativeWorldStore::validDocument(bytes.data(), bytes.size())) {
        *error = "MonoVM produced malformed BWC1 snapshot";
        return false;
    }
    if (activeWorldStore().write(epoch, bytes) != Result::Ok) {
        *error = "Atomic sidecar write rejected";
        return false;
    }
    MonoWorldRequest confirm{};
    confirm.action = MonoWorldAction::Confirm;
    confirm.epoch = epoch;
    confirm.payload = std::move(prepared.payload);
    MonoWorldReply acknowledged{};
    return invoke(std::move(confirm), &acknowledged, error);
}

bool leaveManagedWorld(uint64_t epoch, std::string* error) {
    if (!error) return false;
    error->clear();
    if (epoch == 0) {
        *error = "Invalid world session";
        return false;
    }
    MonoWorldRequest leave{};
    leave.action = MonoWorldAction::Leave;
    leave.epoch = epoch;
    MonoWorldReply reply{};
    const bool ok = invoke(std::move(leave), &reply, error);
    activeWorldStore().endWorld(epoch);
    return ok;
}
}
