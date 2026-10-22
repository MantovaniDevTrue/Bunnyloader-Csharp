#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <bitset>
#include <string>

namespace bl::csharp::hair {

// Snapshot da thread IL2CPP, antes da reserva de IDs ou do Seal.
// Não mantém ponteiros do GC para outra thread; somente bits copiados.
struct NativeHairSlotSnapshot {
    std::bitset<Registry::kLastType + 1> occupied;
    bool valid = false;
    std::string error;

    bool IsOccupied(int type) const {
        return !valid || type < Registry::kVanillaCount ||
               type > Registry::kLastType ||
               occupied.test(static_cast<size_t>(type));
    }
};

// Só chamar na game thread com TextureAssets inicializado. O retorno
// fail-closed evita reservar ID caso o Bunny JS ainda esteja instalando
// texturas ou se o layout das duas tabelas não estiver disponível.
NativeHairSlotSnapshot ReadNativeHairOccupiedSlots();

} // namespace bl::csharp::hair
