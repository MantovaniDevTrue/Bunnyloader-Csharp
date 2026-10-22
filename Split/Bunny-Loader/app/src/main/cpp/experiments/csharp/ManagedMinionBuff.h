#pragma once

#include <cstdint>

namespace bl::csharp {

// Equivale ao UpdatePlayer do ExampleMinionBuff: enquanto o jogador tiver o
// minion, o buff e renovado; sem minion, e removido.
constexpr int32_t kMinionBuffTicks = 18000;

enum class MinionBuffAction { Keep, Refresh, Remove };

// ownedCount < 0: contagem indisponivel (tabela curta ou nula); nao mexer.
inline MinionBuffAction decideMinionBuff(int32_t ownedCount, int32_t buffTime) {
    if (ownedCount < 0) return MinionBuffAction::Keep;
    if (ownedCount > 0)
        return buffTime == kMinionBuffTicks ? MinionBuffAction::Keep
                                            : MinionBuffAction::Refresh;
    return MinionBuffAction::Remove;
}

}  // namespace bl::csharp
