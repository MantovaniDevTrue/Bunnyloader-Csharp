#pragma once

#include <cstdint>
#include <cstddef>

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

// DelBuff shifts entries left. Reverse traversal never executes a shifted buff twice.
template<class Visitor>
inline void visitManagedBuffSlots(int32_t* types,int32_t* times,size_t count,Visitor visitor) {
    for(size_t remaining=count;remaining>0;--remaining) {
        const size_t slot=remaining-1;
        if(types[slot]>0&&times[slot]>0)visitor(slot);
    }
}

}  // namespace bl::csharp
