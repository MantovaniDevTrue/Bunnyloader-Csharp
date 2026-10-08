#pragma once

#include "experiments/csharp/CSharpHostTypes.h"

namespace bl::csharp {

struct NpcAiStatus {
    bool installed = false;
    int calls = 0;
    int writes = 0;
    int spawns = 0;
    int releases = 0;
};

bool installNpcAiHost();
NpcAiStatus npcAiStatus();

}
