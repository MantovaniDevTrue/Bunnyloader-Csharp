#include "experiments/csharp/NativeHairStartup.h"
#include "experiments/csharp/NativeHairSlotMailbox.h"

namespace bl::csharp::hair {

NativeHairStartup MakeNativeHairStartupForGameThread() {
    return NativeHairStartup(ReadNativeHairOccupiedSlots);
}

NativeHairStartup MakeNativeHairStartupForWorker() {
    return NativeHairStartup([] {
        return RequestNativeHairSlotsOnWorker();
    });
}

} // namespace bl::csharp::hair
