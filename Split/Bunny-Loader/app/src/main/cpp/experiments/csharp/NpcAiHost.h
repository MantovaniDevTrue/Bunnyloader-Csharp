#pragma once

#include "experiments/csharp/CSharpHostTypes.h"
#include <string>

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

struct LiveNpcAiStatus {
    bool installed = false;
    bool enabled = false;
    bool limitReached = false;
    int npcType = -1;
    int events = 0;
    int writes = 0;
    int errors = 0;
    int queued = 0;
    int discarded = 0;
    bool workerStarted = false;
    bool workerPending = false;
    int lastSlot = -1;
    int lastDeltaX = 0;
    int lastVelocityTenths = 0;
    double maxCallbackMs = 0.0;
    std::string modId;
    std::string phase = "idle";
};

bool enableLiveNpcAi(const std::string& modId, int npcType,
                     const std::string& assemblyPath, std::string* error);
void pauseLiveNpcAi();
LiveNpcAiStatus liveNpcAiStatus();

bool takeNpcAttack(int slot, int type, HostNpcAttack* out);
void resetManagedSourceNpcSlots();
bool installNpcAiHost();
bool installManagedNpcBuffDispatcher();
void requestManagedNpcBuffDispatcher();
void updateManagedNpcBuffDispatcherOnGameThread();
NpcAiStatus npcAiStatus();

}
