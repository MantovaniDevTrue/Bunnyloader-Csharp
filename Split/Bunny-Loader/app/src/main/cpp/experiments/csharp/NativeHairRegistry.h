#pragma once

#include "experiments/csharp/ContentTexture.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bl::csharp::hair {

enum class RandomGender : int { Unspecified = 0, Male = 1, Female = 2 };

struct Declaration {
    std::string modUid;
    std::string className;
    std::string texture;
    std::string altTexture;
    RandomGender randomGender = RandomGender::Unspecified;
    bool availableDuringCharacterCreation = true;
};

struct Entry {
    int type = -1;
    std::string fullName;
    std::string texture;
    std::string altTexture;
    RandomGender randomGender = RandomGender::Unspecified;
    bool availableDuringCharacterCreation = true;
};

struct Appearance {
    int player = -1;
    int type = 0;
    std::string fullName;
    bool fallback = false;
};

enum class Receiver { Server, Client };

inline bool validIdentifier(std::string_view value) {
    if (value.empty() || value.size() > 128) return false;
    for (const unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

inline bool splitName(std::string_view value, std::string_view* uid, std::string_view* name) {
    const size_t slash = value.find('/');
    if (slash == std::string_view::npos ||
        value.find('/', slash + 1) != std::string_view::npos) return false;
    const auto a = value.substr(0, slash);
    const auto b = value.substr(slash + 1);
    if (!validIdentifier(a) || !validIdentifier(b)) return false;
    if (uid) *uid = a;
    if (name) *name = b;
    return true;
}

class Registry {
public:
    static constexpr int kVanillaCount = 228;
    static constexpr int kLastType = 4095;
    using AssetExists = std::function<bool(std::string_view)>;

    explicit Registry(int firstModType = kVanillaCount)
        : firstModType_(firstModType), nextType_(firstModType) {}

    bool RegisterBatch(const std::vector<Declaration>& batch,
                       const AssetExists& exists, std::string* error) {
        return RegisterAt(batch, nextType_, exists, error);
    }

    // O host reserva blocos globais depois de detectar os IDs usados pelo
    // Bunny JS. As faixas de mods diferentes podem conter buracos, e não
    // devem ser compactadas: salvar e net usam sempre UID/classe.
    bool RegisterAt(const std::vector<Declaration>& batch, int firstReservedType,
                    const AssetExists& exists, std::string* error) {
        if (!error) return false;
        error->clear();
        if (!exists || firstModType_ < kVanillaCount || firstModType_ > kLastType ||
            firstReservedType < kVanillaCount || firstReservedType > kLastType) {
            *error = "NativeHairInvalidCapacityOrAssetProvider";
            return false;
        }
        if (batch.size() > static_cast<size_t>(kLastType - firstReservedType + 1)) {
            *error = "NativeHairCapacityExceeded";
            return false;
        }

        std::vector<Entry> prepared;
        prepared.reserve(batch.size());
        std::set<std::string> added;
        int candidate = firstReservedType;
        for (const auto& item : batch) {
            if (!validIdentifier(item.modUid) || !validIdentifier(item.className)) {
                *error = "NativeHairInvalidIdentity";
                return false;
            }
            const std::string full = item.modUid + "/" + item.className;
            if (byName_.count(full) || !added.insert(full).second) {
                *error = "NativeHairDuplicateIdentity:" + full;
                return false;
            }
            if (item.randomGender != RandomGender::Unspecified &&
                item.randomGender != RandomGender::Male &&
                item.randomGender != RandomGender::Female) {
                *error = "NativeHairInvalidGender:" + full;
                return false;
            }
            if (byType_.contains(candidate)) {
                *error = "NativeHairTypeAlreadyReserved:" + std::to_string(candidate);
                return false;
            }
            std::string main, alt, reason;
            if (!normalizeContentTexture(item.texture.c_str(), &main, &reason)) {
                *error = "NativeHairMainTexture:" + full + ":" + reason;
                return false;
            }
            if (!normalizeContentTexture(item.altTexture.c_str(), &alt, &reason)) {
                *error = "NativeHairAltTexture:" + full + ":" + reason;
                return false;
            }
            if (!exists(main)) {
                *error = "NativeHairAssetMissing:" + full + ":" + main;
                return false;
            }
            if (!exists(alt)) alt = main;
            prepared.push_back({candidate++, full, std::move(main), std::move(alt),
                                item.randomGender, item.availableDuringCharacterCreation});
        }
        for (auto& item : prepared) {
            const int id = item.type;
            byName_.emplace(item.fullName, id);
            byType_.emplace(id, std::move(item));
        }
        nextType_ = std::max(nextType_, candidate);
        return true;
    }

    const Entry* ByType(int type) const {
        const auto it = byType_.find(type);
        return it == byType_.end() ? nullptr : &it->second;
    }

    const Entry* ByName(std::string_view fullName) const {
        const auto it = byName_.find(std::string(fullName));
        return it == byName_.end() ? nullptr : ByType(it->second);
    }

    int Resolve(std::string_view fullName) const {
        const auto* item = ByName(fullName);
        return item ? item->type : 0;
    }

    std::vector<Entry> Snapshot() const {
        std::vector<Entry> result;
        result.reserve(byType_.size());
        for (const auto& [id, value] : byType_) result.push_back(value);
        return result;
    }

    std::string SaveKey(int hairType) const {
        const Entry* item = ByType(hairType);
        return item ? "H1|" + item->fullName : std::string{};
    }

    bool RestoreKey(std::string_view data, int* hairType) const {
        if (!hairType) return false;
        *hairType = 0;
        if (!data.starts_with("H1|")) return false;
        const std::string_view key = data.substr(3);
        if (!splitName(key, nullptr, nullptr)) return false;
        *hairType = Resolve(key);
        return true;
    }

    std::string EncodeAppearance(int player, int hairType) const {
        if (player < 0 || player > 255) return {};
        const Entry* item = ByType(hairType);
        if (!item) return {};
        return "H1|" + std::to_string(player) + "|" + item->fullName;
    }

    bool DecodeAppearance(std::string_view packet, Receiver receiver, int from,
                          Appearance* out) const {
        if (!out || packet.size() > 300 || !packet.starts_with("H1|")) return false;
        const size_t delimiter = packet.find('|', 3);
        if (delimiter == std::string_view::npos) return false;
        const auto number = packet.substr(3, delimiter - 3);
        if (number.empty() || number.size() > 3) return false;
        int player = -1;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), player);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
            player < 0 || player > 255) return false;
        const auto key = packet.substr(delimiter + 1);
        if (!splitName(key, nullptr, nullptr)) return false;
        if (receiver == Receiver::Server) {
            if (from < 0 || from > 255 || player != from || !ByName(key))
                return false;
        } else if (from != 256) {
            return false;
        }
        const int type = Resolve(key);
        *out = {player, type, std::string(key), type == 0};
        return true;
    }

    void Reset() {
        byName_.clear();
        byType_.clear();
        nextType_ = firstModType_;
    }

    int NextType() const { return nextType_; }
    size_t Size() const { return byType_.size(); }

private:
    const int firstModType_;
    int nextType_;
    std::map<int, Entry> byType_;
    std::map<std::string, int, std::less<>> byName_;
};

}  // namespace bl::csharp::hair
