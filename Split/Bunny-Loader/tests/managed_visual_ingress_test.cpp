#include "experiments/csharp/ManagedVisualIngress.h"
#include <cassert>
int main() {
    bl::csharp::ManagedVisualQueue queue;
    using bl::csharp::ManagedVisualWire;
    assert(ManagedVisualWire::accept(queue, "V1|2|0|7|0|0|1|0.5|4294967295|6|shield.png"));
    assert(!ManagedVisualWire::accept(queue, "V1|2|0|7|0|0|1|0.5|4294967295|6|../escape.png"));
    assert(!ManagedVisualWire::accept(queue, "V1|7|0|7|0|0|1|0.5|4294967295|6|shield.png"));
    bl::csharp::ManagedVisualCommand cmd;
    assert(queue.pop(cmd));
    assert(cmd.effectId == 7 && cmd.texture == "shield.png");
}
