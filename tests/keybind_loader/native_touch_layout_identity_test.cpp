#include "experiments/csharp/NativeTouchLayoutIdentity.h"
#include <cassert>
#include <string>

using bl::csharp::touch::storedTouchActionIdentity;

int main() {
    constexpr int unbound = 28;
    assert(storedTouchActionIdentity(541, unbound, 4, "modA/Dash",
                                     "modB/Old", false) == "modA/Dash");
    assert(storedTouchActionIdentity(541, unbound, 4, "", "modA/Dash", false)
           == "modA/Dash");
    assert(storedTouchActionIdentity(unbound, unbound, -1, "",
                                     "modA/Dash", false) == "modA/Dash");
    assert(storedTouchActionIdentity(unbound, unbound, -1, "",
                                     "modA/Dash", true).empty());
    assert(storedTouchActionIdentity(1, unbound, -1, "",
                                     "modA/Dash", false).empty());
    assert(storedTouchActionIdentity(541, unbound, 4, "",
                                     "modA/Dash", true).empty());
    assert(storedTouchActionIdentity(541, unbound, 4, "",
                                     "", false).empty());
    return 0;
}
