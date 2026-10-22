#include "experiments/csharp/NativeHairStartup.h"

namespace bl::csharp::hair {

NativeHairStartup MakeNativeHairStartupForGameThread() {
    return NativeHairStartup(ReadNativeHairOccupiedSlots);
}

} // namespace bl::csharp::hair
