#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <string>
#include <utility>

namespace bl::csharp::hair {

// Guardar o cabelo vanilla no arquivo .plr sem modificar permanentemente o
// Player no mundo. O sidecar guarda UID/classe; o número local não é estável
// entre instalações/ordens de mods. Usar no hook InternalSavePlayerFile.
class VanillaSaveGuard {
public:
    VanillaSaveGuard(int* hairField, const Registry& registry)
        : hairField_(hairField) {
        if (!hairField_) return;
        const Entry* entry = registry.ByType(*hairField_);
        if (!entry) return;
        savedType_ = *hairField_;
        stableKey_ = registry.SaveKey(savedType_);
        if (stableKey_.empty()) return;
        *hairField_ = 0; // Terraria jamais salva um ID de mod inexistente no vanilla
        modified_ = true;
    }

    VanillaSaveGuard(const VanillaSaveGuard&) = delete;
    VanillaSaveGuard& operator=(const VanillaSaveGuard&) = delete;
    VanillaSaveGuard(VanillaSaveGuard&&) = delete;
    VanillaSaveGuard& operator=(VanillaSaveGuard&&) = delete;

    ~VanillaSaveGuard() {
        if (modified_ && hairField_) *hairField_ = savedType_;
    }

    bool Modified() const { return modified_; }
    int OriginalType() const { return modified_ ? savedType_ : 0; }
    const std::string& StableKey() const { return stableKey_; }

private:
    int* hairField_ = nullptr;
    int savedType_ = 0;
    bool modified_ = false;
    std::string stableKey_;
};

// Carregamentos tardios aguardam a instalação de texturas. Não escreva o
// índice de cabelo no Player enquanto TextureAssets.PlayerHair não existir:
// na versão IL2CPP atual não há bounds check neste acesso.
class PendingRestore {
public:
    explicit PendingRestore(std::string stableKey) : stableKey_(std::move(stableKey)) {}

    bool Apply(int* field, const Registry& registry, bool texturesReady) {
        if (!field || !texturesReady || stableKey_.empty()) return false;
        int type = 0;
        if (!registry.RestoreKey(stableKey_, &type)) return false;
        *field = type; // ausente => cabelo vanilla 0
        stableKey_.clear();
        return true;
    }

    const std::string& Key() const { return stableKey_; }

private:
    std::string stableKey_;
};

} // namespace bl::csharp::hair
