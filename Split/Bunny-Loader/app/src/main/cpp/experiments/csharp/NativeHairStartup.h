#pragma once

#include "experiments/csharp/NativeHairSlotProbe.h"
#include "experiments/csharp/NativeHairStaging.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace bl::csharp::hair {

// Coordenador da fase pré-IL2CPP. Recebe snapshots de slots reais da
// game thread. Releitura em Seal detecta cabelos JS instalados durante
// a compilação MonoVM. Sem Seal, os descritores NÃO ficam publicáveis.
class NativeHairStartup {
public:
    using SlotReader = std::function<NativeHairSlotSnapshot()>;
    using Block = NativeHairStaging::Block;

    explicit NativeHairStartup(SlotReader reader)
        : readSlots_(std::move(reader)) {}

    bool Begin(std::string_view uid, int count,
               Block* reserved, std::string* error) {
        if (!error) return false;
        if (!readSlots_) {
            *error = "HairStartupMissingTextureSlotReader";
            return false;
        }
        NativeHairSlotSnapshot snapshot = readSlots_();
        if (!snapshot.valid) {
            *error = snapshot.error.empty() ?
                "HairStartupTextureAssetsUnavailable" : snapshot.error;
            return false;
        }
        return staged_.Begin(uid, count,
            [&snapshot](int id) { return snapshot.IsOccupied(id); },
            reserved, error);
    }

    bool CommitManifest(std::string_view uid, std::string_view manifest,
                        const Registry::AssetExists& assets, std::string* error) {
        return staged_.CommitManifest(uid, manifest, assets, error);
    }

    bool Seal(std::string* error) {
        if (!error) return false;
        if (!readSlots_) {
            *error = "HairStartupMissingTextureSlotReader";
            return false;
        }
        NativeHairSlotSnapshot snapshot = readSlots_();
        if (!snapshot.valid) {
            *error = snapshot.error.empty() ?
                "HairStartupTextureAssetsUnavailable" : snapshot.error;
            return false;
        }
        return staged_.Seal(
            [&snapshot](int id) { return snapshot.IsOccupied(id); },
            error);
    }

    // Só fornece os registros para a futura instalação IL2CPP depois
    // que todos os pacotes de uma sessão foram confirmados no Seal.
    const Registry* ReadyDescriptors() const {
        return staged_.Sealed() ? &staged_.Descriptors() : nullptr;
    }

    Block Reservation(std::string_view uid) const {
        return staged_.Reservation(uid);
    }

    bool AbortUncommitted(std::string_view uid) {
        return staged_.AbortUncommitted(uid);
    }

    void Reset() { staged_.Reset(); }

private:
    SlotReader readSlots_;
    NativeHairStaging staged_;
};

// O host utiliza o probe IL2CPP existente; os testes fornecem snapshots
// isolados e não dependem de Terraria/GameAssembly.
NativeHairStartup MakeNativeHairStartupForGameThread();

} // namespace bl::csharp::hair
