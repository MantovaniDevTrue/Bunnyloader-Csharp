#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <map>
#include <vector>

namespace bl::csharp::hair {

// No tModLoader, o resultado de GetUnlockConditions é reavaliado quando
// a interface pergunta por cabelos disponíveis. Não se deve cachear
// permanentemente a progressão de um mundo ou reutilizar a do Stylist
// na criação de personagem.
using UnlockQuery = bool (*)(void* context, const Entry& entry,
                             bool atCreation, bool atStylist, bool* unlocked);

class UnlockState {
public:
    bool Refresh(const Registry& registry, bool creation, bool stylist,
                 UnlockQuery query, void* context) {
        const auto entries = registry.Snapshot();
        std::map<int, bool> current;
        for (const auto& entry : entries) {
            bool unlocked = false;
            if (creation) {
                unlocked = entry.availableDuringCharacterCreation;
            } else if (stylist) {
                // Sem callback de condições do Mono, não mostrar conteúdo
                // ainda não suportado; nunca declarar liberado por acidente.
                if (!query || !query(context, entry, false, true, &unlocked))
                    unlocked = false;
            }
            current.emplace(entry.type, unlocked);
        }
        const bool changed = !initialized_ || creation != creation_ ||
            stylist != stylist_ || current != previous_;
        previous_ = std::move(current);
        creation_ = creation;
        stylist_ = stylist;
        initialized_ = true;
        return changed;
    }

    bool IsAvailable(int type) const {
        const auto found = previous_.find(type);
        return found != previous_.end() && found->second;
    }

    std::vector<int> AvailableTypes() const {
        std::vector<int> result;
        for (const auto& [type, unlocked] : previous_)
            if (unlocked) result.push_back(type);
        return result;
    }

    void Reset() {
        previous_.clear();
        initialized_ = false;
        creation_ = false;
        stylist_ = false;
    }

private:
    bool initialized_ = false;
    bool creation_ = false;
    bool stylist_ = false;
    std::map<int, bool> previous_;
};

} // namespace bl::csharp::hair
