#include "experiments/csharp/ManagedEntityBindValidation.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    const uint8_t yes = 1;
    const uint8_t no = 0;
    const uint8_t unexpected = 2;
    assert(bl::csharp::managedEntityBindAccepted(&yes));
    assert(!bl::csharp::managedEntityBindAccepted(&no));
    assert(!bl::csharp::managedEntityBindAccepted(&unexpected));
    assert(!bl::csharp::managedEntityBindAccepted(nullptr));
    std::cout << "MANAGED_ENTITY_BIND_VALIDATION_PASS\n";
}
