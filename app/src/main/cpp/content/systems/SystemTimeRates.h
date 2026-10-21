#pragma once
#include <cmath>
#include <string>

namespace bl::runtime::systems {
struct TimeRates { double time=1, tiles=1, events=1; };
inline bool validTimeRate(double rate) {
    return std::isfinite(rate) && rate>=0 && rate<=2147483647.0;
}
// prepareTimeRates resolves metadata only on an attached loading thread.
// Transactions are game thread only; clockRate() is an atomic snapshot.
// Bracket each language's native
// UpdateTimeRate original, compose full-precision values, then commit once at
// the outermost boundary. No Mono or QuickJS dependency belongs in this service.
bool prepareTimeRates(std::string* error);
bool beginTimeRates(std::string* error);
bool readTimeRates(TimeRates* rates,std::string* error);
bool publishTimeRates(TimeRates rates,std::string* error);
bool endTimeRates(std::string* error);
double clockRate();
void observeTimeRateMenu(bool menu);
}
