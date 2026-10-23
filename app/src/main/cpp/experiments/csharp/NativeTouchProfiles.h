#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "experiments/csharp/NativeTouchInput.h"

namespace bl::csharp::touch {

struct SlotAction {
    int action = -1;
    int style = -1;
    int groupCount = 0;
    std::array<int, 16> group{};
};

struct ActiveTouchSnapshot {
    std::array<bool, kMaxButtons> bound{};
    std::array<int, kMaxButtons> actionSlots{};
    bool ready = false;
};

// Lê somente estruturas existentes do jogo. Falha fechada quando uma
// propriedade/campo muda na versão do Terraria ou usa layout não validado.
int nativeTouchActiveSlotCount();
bool nativeTouchSlot(int slot, SlotAction* result);
bool nativeTouchSlotId(int slot, std::string* id);
const void* nativeTouchActiveProfileIdentity();
bool nativeTouchActiveSnapshot(ActiveTouchSnapshot* result);

} // namespace bl::csharp::touch
