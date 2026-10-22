#pragma once
#include <string>
namespace bl::runtime {
// Native service: no JS values, Mono objects or VM callbacks are needed.
// Restores both live fade arrays together, including after LocalUserGameState
// is replaced. Call only on the observed engine thread before DrawWaters.
bool ensureWaterAlphaTables(int total,std::string* error);
}
