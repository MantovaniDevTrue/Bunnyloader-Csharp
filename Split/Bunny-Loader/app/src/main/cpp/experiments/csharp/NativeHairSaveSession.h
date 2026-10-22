#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <map>
#include <string>
#include <string_view>

namespace bl::csharp::hair {

// Sidecar externo não pode ser apagado por um mod C# ausente: ao carregar
// sua chave, o jogador recebe cabelo vanilla 0 até o mod voltar.
// Porém escolher um novo cabelo manualmente cancela a identidade antiga.
enum class SidecarAction { Preserve, Update, Remove };

struct SaveDecision {
    SidecarAction action = SidecarAction::Preserve;
    std::string stableKey;
};

class PlayerHairSaveSession {
public:
    // Chamado após ler o sidecar do personagem. localType é o tipo resolvido
    // na sessão atual (0 para mod ausente). Não armazena ponteiros IL2CPP.
    void OnLoaded(std::string_view playerPath, std::string_view stableKey,
                  int localType) {
        if (playerPath.empty() || !stableKey.starts_with("H1|") ||
            !splitName(stableKey.substr(3), nullptr, nullptr)) return;
        saved_[std::string(playerPath)] = {std::string(stableKey), localType};
    }

    SaveDecision BeforeSave(std::string_view playerPath, int selectedType,
                            const Registry& registry) const {
        if (const Entry* known = registry.ByType(selectedType))
            return {SidecarAction::Update, registry.SaveKey(known->type)};

        const auto found = saved_.find(std::string(playerPath));
        if (found == saved_.end()) return {};
        if (selectedType == found->second.localType) return {};

        // Seleção manual de cabelo vanilla ou de outro loader substitui
        // o penteado antigo. Só remover após o save vanilla ter sucesso.
        return {SidecarAction::Remove, {}};
    }

    // Aplicar a decisão só depois que a gravação da .plr e o sidecar
    // tiverem terminado. Uma falha de IO nunca pode alterar o tracker.
    void Commit(std::string_view path, const SaveDecision& decision,
                int selectedType) {
        if (decision.action == SidecarAction::Update)
            OnLoaded(path, decision.stableKey, selectedType);
        else if (decision.action == SidecarAction::Remove)
            saved_.erase(std::string(path));
    }

    bool HasPending(std::string_view path) const {
        return saved_.find(std::string(path)) != saved_.end();
    }

    void Reset() { saved_.clear(); }

private:
    struct EntryState {
        std::string stableKey;
        int localType = 0;
    };
    std::map<std::string, EntryState, std::less<>> saved_;
};

} // namespace bl::csharp::hair
