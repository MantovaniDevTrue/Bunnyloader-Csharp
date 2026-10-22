#pragma once

#include "experiments/csharp/ManagedBannerRegistry.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::banners {

struct WorldCounter {
    int32_t kills = 0;
    uint16_t claimable = 0;
    bool fresh = false;

    bool operator==(const WorldCounter&) const = default;
    bool empty() const { return kills == 0 && claimable == 0; }
};

using WorldEntries = std::map<std::string, WorldCounter>;

class WorldLedger final {
public:
    static constexpr uint32_t kMaxEntries = 4096;
    static constexpr size_t kMaxBytes = 1048576;

    // Chaves iguais as do BannerWorldSystem original: UUID/NomeDaClasse.
    // Nunca guardar tipo numerico, que muda quando a ordem dos mods muda.
    static bool keyValid(const std::string& key) {
        if (key.empty() || key.size() > 260) return false;
        auto slash = key.find('/');
        if (slash == std::string::npos || slash == 0 ||
            slash == key.size() - 1 ||
            key.find('/', slash + 1) != std::string::npos) return false;
        auto part = [](const std::string& text, size_t first, size_t end) {
            if (end - first > 128 || end == first) return false;
            for (size_t i = first; i < end; ++i) {
                char ch = text[i];
                if (!((ch >= 'a' && ch <= 'z') ||
                      (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') ||
                      ch == '_' || ch == '-' || ch == '.')) return false;
            }
            return text.substr(first, end - first) != "." &&
                   text.substr(first, end - first) != "..";
        };
        return part(key, 0, slash) && part(key, slash + 1, key.size());
    }

    static uint32_t checksum(const std::vector<uint8_t>& bytes) {
        uint32_t crc = 0xffffffffu;
        for (uint8_t b : bytes) {
            crc ^= b;
            for (int i = 0; i < 8; ++i)
                crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    }

    // BNR1 = assinatura + contagem uint16 + registros + CRC32.
    // Campos numericos sao little-endian, sem ponteiros ou structs IL2CPP.
    static bool encode(const WorldEntries& entries, std::vector<uint8_t>* result) {
        if (!result || entries.size() > kMaxEntries) return false;
        std::vector<uint8_t> bytes{'B', 'N', 'R', '1'};
        append16(bytes, static_cast<uint16_t>(entries.size()));
        for (const auto& [key, state] : entries) {
            if (!keyValid(key) || state.kills < 0 ||
                state.claimable == 0 && state.fresh || bytes.size() + key.size() + 12 > kMaxBytes)
                return false;
            append16(bytes, static_cast<uint16_t>(key.size()));
            bytes.insert(bytes.end(), key.begin(), key.end());
            append32(bytes, static_cast<uint32_t>(state.kills));
            append16(bytes, state.claimable);
            bytes.push_back(static_cast<uint8_t>(state.fresh));
        }
        const uint32_t crc = checksum(bytes);
        append32(bytes, crc);
        if (bytes.size() > kMaxBytes) return false;
        *result = std::move(bytes);
        return true;
    }

    // Qualquer erro deixa *result intacto; formato nao aceita lixo sobrando,
    // chaves duplicadas, flags invalidas ou checksum incorreto.
    static bool decode(const std::vector<uint8_t>& data, WorldEntries* result) {
        if (!result || data.size() < 10 || data.size() > kMaxBytes ||
            data[0] != 'B' || data[1] != 'N' || data[2] != 'R' || data[3] != '1')
            return false;
        size_t pos = data.size() - 4;
        uint32_t wanted = 0;
        if (!read32(data, &pos, &wanted) || wanted != checksum(
                std::vector<uint8_t>(data.begin(), data.end() - 4))) return false;

        pos = 4;
        uint16_t count = 0;
        if (!read16(data, &pos, &count) || count > kMaxEntries) return false;
        WorldEntries staged;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t length = 0;
            uint32_t kills = 0;
            uint16_t claimable = 0;
            if (!read16(data, &pos, &length) || length == 0 ||
                length > 260 || pos + length > data.size() - 4) return false;
            std::string key(data.begin() + pos, data.begin() + pos + length);
            pos += length;
            if (!keyValid(key) || !read32(data, &pos, &kills) ||
                !read16(data, &pos, &claimable) ||
                pos >= data.size() - 4 || kills > INT32_MAX) return false;
            uint8_t fresh = data[pos++];
            if (fresh > 1 || fresh && !claimable ||
                !staged.emplace(std::move(key), WorldCounter{
                    static_cast<int32_t>(kills), claimable, fresh != 0}).second)
                return false;
        }
        if (pos != data.size() - 4) return false;
        *result = std::move(staged);
        return true;
    }

    // O progresso de mods ausentes fica na copia preservada. Para o mod
    // presente, o array do BannerSystem e a fonte de verdade.
    static bool capture(const Snapshot& bound, const std::vector<int32_t>& kills,
                        const std::vector<int32_t>& claimable,
                        const std::vector<uint8_t>& fresh,
                        WorldEntries* retained) {
        if (!retained) return false;
        auto staged = *retained;
        if (staged.size() > kMaxEntries) return false;
        for (const auto& [modId, definitions] : bound.byMod) {
            for (const auto& b : definitions) {
                int id = b.representativeType;
                if (id < 0 || static_cast<size_t>(id) >= kills.size() ||
                    static_cast<size_t>(id) >= claimable.size() ||
                    static_cast<size_t>(id) >= fresh.size()) return false;
            }
        }
        for (const auto& [modId, definitions] : bound.byMod) {
            for (const auto& b : definitions) {
                int id = b.representativeType;
                const std::string key = modId + "/" + b.npcClass;
                if (!keyValid(key)) return false;
                WorldCounter state{
                    std::max(0, kills[id]),
                    static_cast<uint16_t>(std::clamp(claimable[id], 0, 65535)),
                    claimable[id] > 0 && fresh[id] != 0
                };
                if (state.empty()) staged.erase(key);
                else staged[key] = state;
            }
        }
        if (staged.size() > kMaxEntries) return false;
        *retained = std::move(staged);
        return true;
    }

    // Preflight de todos os indices antes de escrever qualquer byte.
    // Arrays vanilla nunca sao tocados; duas variantes compartilham o maior
    // contador salvo para evitar restaurar valores em ordem errada.
    static bool restore(const Snapshot& bound, const WorldEntries& saved,
                        std::vector<int32_t>* kills,
                        std::vector<int32_t>* claimable,
                        std::vector<uint8_t>* fresh) {
        if (!kills || !claimable || !fresh ||
            saved.size() > kMaxEntries) return false;
        for (const auto& [key, state] : saved)
            if (!keyValid(key) || state.kills < 0 ||
                (state.claimable == 0 && state.fresh))
                return false;
        for (const auto& [id, item] : bound.bannerToItem) {
            if (id < 0 || static_cast<size_t>(id) >= kills->size() ||
                static_cast<size_t>(id) >= claimable->size() ||
                static_cast<size_t>(id) >= fresh->size()) return false;
        }
        auto nextKills = *kills;
        auto nextClaimable = *claimable;
        auto nextFresh = *fresh;
        for (const auto& [id, item] : bound.bannerToItem) {
            nextKills[id] = 0; nextClaimable[id] = 0; nextFresh[id] = 0;
        }
        for (const auto& [modId, defs] : bound.byMod) {
            for (const auto& b : defs) {
                auto found = saved.find(modId + "/" + b.npcClass);
                if (found == saved.end()) continue;
                int id = b.representativeType;
                const auto& state = found->second;
                nextKills[id] = std::max(nextKills[id], state.kills);
                nextClaimable[id] = std::max(nextClaimable[id],
                                             static_cast<int32_t>(state.claimable));
                nextFresh[id] = nextFresh[id] || (state.fresh && state.claimable);
            }
        }
        *kills = std::move(nextKills);
        *claimable = std::move(nextClaimable);
        *fresh = std::move(nextFresh);
        return true;
    }

private:
    static void append16(std::vector<uint8_t>& v, uint16_t value) {
        v.push_back(static_cast<uint8_t>(value));
        v.push_back(static_cast<uint8_t>(value >> 8));
    }
    static void append32(std::vector<uint8_t>& v, uint32_t value) {
        for (int i = 0; i < 4; ++i)
            v.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
    static bool read16(const std::vector<uint8_t>& b, size_t* p, uint16_t* v) {
        if (!p || !v || *p > b.size() || b.size() - *p < 2) return false;
        *v = static_cast<uint16_t>(b[*p]) |
             static_cast<uint16_t>(b[*p + 1]) << 8;
        *p += 2;
        return true;
    }
    static bool read32(const std::vector<uint8_t>& b, size_t* p, uint32_t* v) {
        if (!p || !v || *p > b.size() || b.size() - *p < 4) return false;
        *v = 0;
        for (int i = 0; i < 4; ++i)
            *v |= static_cast<uint32_t>(b[*p + i]) << (8 * i);
        *p += 4;
        return true;
    }
};

} // namespace bl::csharp::banners
