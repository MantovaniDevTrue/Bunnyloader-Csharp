#pragma once

#include "experiments/csharp/CSharpHostTypes.h"

namespace bl::csharp {

struct NpcAiStatus {
    bool installed = false;
    int calls = 0;
    int writes = 0;
    int spawns = 0;
    int releases = 0;
    int attacksQueued = 0;
    int attacksConsumed = 0;
    int attacksRejected = 0;
};

bool takeNpcAttack(int slot, int type, HostNpcAttack* out);
bool installNpcAiHost();
NpcAiStatus npcAiStatus();

}
