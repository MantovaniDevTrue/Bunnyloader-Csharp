#include "experiments/csharp/CompilerAot.h"
#include <cassert>
#include <iostream>
#include <filesystem>

static int registrations = 0;
static void record(void** info) { assert(info); ++registrations; }
int main(int argc, char** argv) {
    assert(argc == 5);
    const std::string mode = argv[1];
    const auto result = bl::csharp::installCompilerAot(argv[2], argv[3], argv[4], record);
    if (mode == "valid") {
        assert(result.registered == 2 && registrations == 2 && result.handle);
        assert(result.reason == "Registered");
    } else {
        assert(!result.handle && !result.registered && !registrations);
        assert(result.reason == mode);
    }
    std::cout << "COMPILER_AOT_LOADER_CONTRACT=" << mode << " PASS\n";
}
