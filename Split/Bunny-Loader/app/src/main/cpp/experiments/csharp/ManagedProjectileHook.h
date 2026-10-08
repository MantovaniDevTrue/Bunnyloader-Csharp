#pragma once

#include <string>

namespace bl::csharp {

struct ManagedProjectileHookStatus {
    bool installed = false;
    bool enabled = false;
    int type = -1;
    int callbacks = 0;
    int velocityWrites = 0;
    int spawns = 0;
    int errors = 0;
    int lastSlot = -1;
    int lastState = 0;
};

bool enableManagedSourceBoltHook(std::string* error);
ManagedProjectileHookStatus managedSourceBoltHookStatus();

}
