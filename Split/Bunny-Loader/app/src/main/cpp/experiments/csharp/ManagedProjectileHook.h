#pragma once

#include <string>
#include <array>

namespace bl::csharp {

struct ManagedProjectileHookStatus {
    bool installed = false;
    bool enabled = false;
    bool buffered = false;
    int bufferSamples = 0;
    int type = -1;
    int callbacks = 0;
    int velocityWrites = 0;
    int spawns = 0;
    int errors = 0;
    int lastSlot = -1;
    int lastState = 0;
};

bool enableManagedSourceBoltHook(std::string* error);
bool enableBufferedSourceBoltHook(const std::array<int, 120>& turnsMilliRadians,
                                  std::string* error);
bool pauseManagedSourceBoltHook();
ManagedProjectileHookStatus managedSourceBoltHookStatus();

}
