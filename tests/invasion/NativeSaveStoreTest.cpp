#include "../../app/src/main/cpp/experiments/csharp/InvasionSaveFile.h"
#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionPersistence.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

extern "C" int bunny_invasion_state_read(int64_t, const char*, const char*, int*, int*, int*);
extern "C" int bunny_invasion_state_write(int64_t, const char*, const char*, int, int, int);

int main() {
    using namespace bl::csharp;
    char temp[] = "/tmp/bunny-invasion-world-XXXXXX";
    char* dir = mkdtemp(temp);
    assert(dir);
    const std::string a = std::string(dir) + "/alpha.wld";
    const std::string b = std::string(dir) + "/beta.wld";
    const std::string fileA = a + ".invasions.bl";
    const std::string fileB = b + ".invasions.bl";
    int phase = -1, progress = -1, cooldown = -1;

    setNativeInvasionSaveWorld(true, 11, a, false);
    assert(!bunny_invasion_state_read(11, "mod-a", "event-a",
                                     &phase, &progress, &cooldown));
    assert(bunny_invasion_state_write(11, "mod-a", "event-a", 1, 7, 90));
    assert(bunny_invasion_state_write(11, "mod-b", "event-b", 2, 40, 0));
    assert(bunny_invasion_state_read(11, "mod-a", "event-a",
                                    &phase, &progress, &cooldown));
    assert(phase == 1 && progress == 7 && cooldown == 90);
    assert(!bunny_invasion_state_write(10, "mod-a", "event-a", 1, 20, 90));
    assert(!bunny_invasion_state_write(11, "../escape", "event-a", 1, 20, 90));
    assert(!bunny_invasion_state_write(11, "mod-a", "event-a", 3, 20, 90));

    setNativeInvasionSaveWorld(false, 12, {}, false);
    assert(!bunny_invasion_state_write(11, "mod-a", "event-a", 1, 20, 90));
    setNativeInvasionSaveWorld(true, 12, b, false);
    assert(!bunny_invasion_state_read(12, "mod-a", "event-a",
                                     &phase, &progress, &cooldown));
    assert(bunny_invasion_state_write(12, "mod-a", "event-a", 1, 13, 0));
    setNativeInvasionSaveWorld(true, 13, a, false);
    assert(bunny_invasion_state_read(13, "mod-a", "event-a",
                                    &phase, &progress, &cooldown));
    assert(phase == 1 && progress == 7 && cooldown == 90);
    assert(bunny_invasion_state_read(13, "mod-b", "event-b",
                                    &phase, &progress, &cooldown));
    assert(phase == 2 && progress == 40);

    assert(bunny_invasion_state_write(13, "mod-a", "event-a", 2, 40, 0));
    assert(bunny_invasion_state_write(13, "mod-a", "event-a", 0, 0, 0));
    assert(!bunny_invasion_state_read(13, "mod-a", "event-a",
                                     &phase, &progress, &cooldown));
    assert(bunny_invasion_state_read(13, "mod-b", "event-b",
                                    &phase, &progress, &cooldown));
    assert(phase == 2);
    setNativeInvasionSaveWorld(true, 14, a, true);
    assert(!bunny_invasion_state_write(14, "mod-a", "event-a", 1, 1, 0));
    setNativeInvasionSaveWorld(true, 15, a, false);
    {
        std::ofstream damaged(fileA, std::ios::binary | std::ios::trunc);
        damaged << "incorrect version\n";
    }
    assert(!bunny_invasion_state_write(15, "mod-a", "event-a", 1, 1, 0));
    {
        std::ifstream verify(fileA, std::ios::binary);
        std::string first;
        std::getline(verify, first);
        assert(first == "incorrect version");
    }
    std::remove(fileA.c_str());
    std::remove(fileB.c_str());
    std::string tempFile = fileA + ".tmp";
    std::remove(tempFile.c_str());
    tempFile = fileB + ".tmp";
    std::remove(tempFile.c_str());
    assert(std::remove(a.substr(0, a.rfind('/')).c_str()) == 0);
    return 0;
}
