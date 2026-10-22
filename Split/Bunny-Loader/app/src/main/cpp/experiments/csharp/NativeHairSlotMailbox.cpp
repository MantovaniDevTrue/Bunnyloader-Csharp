#include "experiments/csharp/NativeHairSlotMailbox.h"

namespace bl::csharp::hair {
namespace {
NativeHairSlotMailbox mailbox;
}

NativeHairSlotSnapshot RequestNativeHairSlotsOnWorker(
    std::chrono::milliseconds timeout) {
    return mailbox.Request(timeout);
}

void PumpNativeHairSlotsOnGameThread() {
    mailbox.Pump(ReadNativeHairOccupiedSlots);
}

void CancelNativeHairSlotRequests() {
    mailbox.Cancel();
}

} // namespace bl::csharp::hair
