#include "experiments/csharp/MonoNativeSignalChaining.h"
#include <cassert>
int sequence = 0;
void signalPolicy(int enabled) { assert(sequence++ == 0 && enabled == 1); }
void crashPolicy(int enabled) { assert(sequence++ == 1 && enabled == 0); }
int main() {
    assert(!bl::csharp::configureMonoNativeSignalChaining(nullptr, crashPolicy));
    assert(!bl::csharp::configureMonoNativeSignalChaining(signalPolicy, nullptr));
    assert(sequence == 0);
    assert(bl::csharp::configureMonoNativeSignalChaining(signalPolicy, crashPolicy));
    assert(sequence == 2);
}
