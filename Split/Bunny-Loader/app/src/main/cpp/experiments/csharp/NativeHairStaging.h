#pragma once

#include "experiments/csharp/NativeHairManifest.h"
#include "experiments/csharp/NativeHairReservation.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace bl::csharp::hair {

// Prepara conteúdo SEM tocar em TextureAssets ou instalar hooks.
// Begin: reserva IDs reais depois dos cabelos do Bunny/vanilla.
// CommitManifest: importa somente o UID autorizado pelo host, em lote.
// Seal: revalida slots porque JS pode ter registrado cabelo durante Roslyn.
// Apenas depois de Seal um futuro coordenador de game-thread poderá instalar
// texturas, UI, save e rede; esta classe não habilita nenhum desses recursos.
class NativeHairStaging {
public:
    using Occupied = TypeReservations::Occupied;
    using AssetExists = Registry::AssetExists;
    using Block = TypeReservations::Block;

    bool Begin(std::string_view modUid, int classCount,
               const Occupied& occupied, Block* block, std::string* error) {
        if (!block || !error) return false;
        *block = {};
        error->clear();
        if (sealed_) {
            *error = "HairStagingAlreadySealed";
            return false;
        }
        if (!validIdentifier(modUid) || mods_.contains(std::string(modUid))) {
            *error = "HairStagingDuplicateOrInvalidMod";
            return false;
        }
        if (!reservations_.Reserve(modUid, classCount, occupied, block, error))
            return false;
        mods_.emplace(std::string(modUid), Mod{*block, false});
        return true;
    }

    bool CommitManifest(std::string_view modUid, std::string_view manifest,
                        const AssetExists& assets, std::string* error) {
        if (!error) return false;
        error->clear();
        if (sealed_) {
            *error = "HairStagingAlreadySealed";
            return false;
        }
        auto found = mods_.find(std::string(modUid));
        if (found == mods_.end() || found->second.registered) {
            *error = "HairStagingMissingOrCommittedMod";
            return false;
        }

        // O manifesto veio de um processo MonoVM separado. Nem o UID nem
        // o número de linhas são confiáveis sem o check cruzado do host.
        constexpr std::string_view prefix = "BUNNY_HAIR_MANIFEST_V1\n";
        if (manifest.size() < prefix.size() || !manifest.starts_with(prefix)) {
            *error = "HairStagingManifestHeaderInvalid";
            return false;
        }
        int lines = 0;
        size_t cursor = prefix.size();
        while (cursor < manifest.size()) {
            const auto end = manifest.find('\n', cursor);
            if (end == std::string_view::npos) {
                *error = "HairStagingIncompleteManifest";
                return false;
            }
            auto line = manifest.substr(cursor, end - cursor);
            const auto firstTab = line.find('\t');
            if (firstTab != 4 || !line.starts_with("HAIR")) {
                *error = "HairStagingRecordInvalid";
                return false;
            }
            const auto second = line.find('\t', firstTab + 1);
            const auto third = second == std::string_view::npos
                ? second : line.find('\t', second + 1);
            if (third == std::string_view::npos ||
                line.substr(second + 1, third - second - 1) != modUid) {
                *error = "HairStagingForeignModIdentity";
                return false;
            }
            ++lines;
            if (lines > found->second.block.count) {
                *error = "HairStagingCountMismatch";
                return false;
            }
            cursor = end + 1;
        }
        if (lines != found->second.block.count) {
            *error = "HairStagingCountMismatch";
            return false;
        }

        Registry next = registry_;
        if (!ParseAndRegisterManifest(manifest, &next, assets, error,
                                      found->second.block.first)) return false;
        registry_.Swap(next);
        found->second.registered = true;
        return true;
    }

    bool Seal(const Occupied& occupied, std::string* error) {
        if (!error) return false;
        error->clear();
        if (sealed_ || !occupied || mods_.empty()) {
            *error = "HairStagingInvalidSeal";
            return false;
        }
        for (const auto& [uid, mod] : mods_) {
            if (!mod.registered) {
                *error = "HairStagingUncommittedMod:" + uid;
                return false;
            }
            for (int offset = 0; offset < mod.block.count; ++offset) {
                const int type = mod.block.first + offset;
                if (!registry_.ByType(type) || occupied(type)) {
                    *error = "HairStagingOccupiedOrMissingType:" +
                        std::to_string(type);
                    return false;
                }
            }
        }
        sealed_ = true;
        return true;
    }

    bool AbortUncommitted(std::string_view uid) {
        if (sealed_) return false;
        auto it = mods_.find(std::string(uid));
        if (it == mods_.end() || it->second.registered) return false;
        reservations_.Release(uid);
        mods_.erase(it);
        return true;
    }

    void Reset() {
        sealed_ = false;
        registry_.Reset();
        reservations_.Reset();
        mods_.clear();
    }

    const Registry& Descriptors() const { return registry_; }
    Block Reservation(std::string_view uid) const {
        return reservations_.ForMod(uid);
    }
    bool Sealed() const { return sealed_; }
    size_t PendingCount() const { return mods_.size() - registryModCount(); }

private:
    struct Mod {
        Block block;
        bool registered = false;
    };
    size_t registryModCount() const {
        size_t count = 0;
        for (const auto& [uid, mod] : mods_) {
            (void)uid;
            if (mod.registered) ++count;
        }
        return count;
    }

    Registry registry_;
    TypeReservations reservations_;
    std::map<std::string, Mod, std::less<>> mods_;
    bool sealed_ = false;
};

} // namespace bl::csharp::hair
