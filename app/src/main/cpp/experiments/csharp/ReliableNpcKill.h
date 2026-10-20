#pragma once
#include <cstddef>
#include <cstdint>

namespace bl::csharp {

bool queueReliableNpcKill(int type, int slot, int generation, uintptr_t identity);
void pumpReliableNpcKillsOnGameThread();
void resetReliableNpcKills();
size_t pendingReliableNpcKills();
uint64_t droppedReliableNpcKills();

}
