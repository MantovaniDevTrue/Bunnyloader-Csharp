#include "experiments/csharp/WorldSystemSaveFile.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    using namespace bl::csharp::world_save;
    const std::string path = "/tmp/bunny-world-system-save-smoke.bl";
    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
    std::string value;

    assert(readState(path, "modA", "World", &value) == 0);
    assert(writeState(path, "modA", "World", "bunny-world-data 1\nboss\tb1\n"));
    assert(writeState(path, "modB", "World", "bunny-world-data 1\nboss\tb0\n"));
    assert(writeState(path, "modA", "World2", "bunny-world-data 1\nscore\ti42\n"));

    assert(readState(path, "modA", "World", &value) == 1 &&
           value == "bunny-world-data 1\nboss\tb1\n");
    assert(readState(path, "modB", "World", &value) == 1 &&
           value == "bunny-world-data 1\nboss\tb0\n");
    assert(readState(path, "modA", "World2", &value) == 1 &&
           value == "bunny-world-data 1\nscore\ti42\n");
    assert(readState(path, "modC", "World", &value) == 0);
    assert(!writeState(path, "../../bad", "World", "bunny-world-data 1\n"));
    assert(!writeState(path, "modA", "World", "invalid"));
    assert(!writeState(path, "modA", "World", std::string(16385, 'x')));
    assert(writeState(path, "modA", "World", "bunny-world-data 1\nboss\tb1\n"));

    {
        std::ofstream file(path, std::ios::trunc);
        file << "corrupt\n";
    }
    assert(readState(path, "modA", "World", &value) == -1);
    assert(!writeState(path, "modA", "World", "bunny-world-data 1\nboss\tb0\n"));
    std::remove(path.c_str());
    std::cout << "Bunny Engine world persistence: PASS\n";
    return 0;
}
