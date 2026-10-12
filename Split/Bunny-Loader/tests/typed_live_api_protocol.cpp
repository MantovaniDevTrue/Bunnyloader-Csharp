#include "experiments/csharp/ManagedNpcBuffEffects.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1]);
    assert(input);
    int encoded = 0, expectedBuff = 0, expectedDuration = 0, count = 0;
    while (input >> encoded >> expectedBuff >> expectedDuration) {
        int buff = -1, duration = -1;
        assert(bl::csharp::decodeManagedNpcBuffCode(encoded, &buff, &duration));
        assert(buff == expectedBuff && duration == expectedDuration);
        ++count;
    }
    assert(input.eof() && count == 4 * (600 - 30 + 1));
    int buff = -1, duration = -1;
    assert(!bl::csharp::decodeManagedNpcBuffCode(0, &buff, &duration));
    std::cout << "TYPED_SDK_NATIVE_PROTOCOL_PARITY=PASS vectors=" << count << '\n';
}
