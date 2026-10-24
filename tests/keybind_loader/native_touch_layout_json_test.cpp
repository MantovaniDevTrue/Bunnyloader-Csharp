#include "experiments/csharp/NativeTouchLayoutJson.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
using namespace bl::csharp::touch;

int main() {
    SavedProfiles profiles;
    assert(readTouchLayoutJson("{\"version\":1,\"profiles\":{}}", &profiles));
    assert(profiles.empty());
    profiles["Default"] = {
        SavedSlot{"slot/one", 2, "example/RandomBuff",
                  {"example/Dash", "", "second/Blink"}},
        SavedSlot{"#3", 3, "", {"second/Jump"}}
    };
    const std::string json = writeTouchLayoutJson(profiles);
    assert(json.find("\"version\":1") != std::string::npos);
    assert(json.find("\"group\"") != std::string::npos);
    assert(json.find("null") != std::string::npos);
    SavedProfiles loaded;
    assert(readTouchLayoutJson(json, &loaded));
    assert(loaded.size() == 1);
    assert(loaded["Default"].size() == 2);
    assert(loaded["Default"][0].action == "example/RandomBuff");
    assert(loaded["Default"][0].group[1].empty());
    assert(loaded["Default"][1].group[0] == "second/Jump");

    const std::string old = writeTouchLayoutJson(loaded);
    for (const char* malformed : {
        "{\"version\":2,\"profiles\":{}}",
        "{\"version\":1,\"profiles\":[]}",
        "{\"version\":1,\"profiles\":{\"a\":[{\"slot\":\"S\",\"index\":1,\"action\":\"../bad\",\"group\":[]}]}}",
        "{\"version\":1,\"profiles\":{\"a\":[{\"slot\":\"S\",\"index\":1},{\"slot\":\"S\",\"index\":2}]}}",
        "{\"version\":1,\"profiles\":{\"a\":[{\"slot\":\"S\",\"index\":999}]}}",
        "{\"version\":1,\"profiles\":{\"a\":[{\"slot\":\"S\",\"index\":1,\"group\":[\"bad\"]}]}}",
        "{\"version\":1,\"profiles\":{\"bad\\nname\":[]}}",
        "{\"version\":1,\"profiles\":{\"a\":[{\"slot\":\"S\",\"index\":1,\"action\":7}]}}",
        "{\"version\":1,\"profiles\":{\"a\":[]},\"version\":1}",
        "{\"version\":1,\"profiles\":{\"a\":[]}}garbage"
    }) {
        assert(!readTouchLayoutJson(malformed, &loaded));
    }
    assert(writeTouchLayoutJson(profiles) == old);

    char temp[] = "/tmp/bunny-touch-json-XXXXXX";
    char* dir = mkdtemp(temp);
    assert(dir);
    assert(loadTouchLayoutFile(dir, &loaded) && loaded.empty());
    assert(saveTouchLayoutFile(dir, profiles));
    assert(loadTouchLayoutFile(dir, &loaded) && loaded["Default"].size() == 2);
    assert(!saveTouchLayoutFile("../unsafe", profiles));
    const std::string path = std::string(dir) + "/BunnyLoader.touchbuttons.csharp.json";
    assert(unlink(path.c_str()) == 0);
    const std::string guard = std::string(dir) + "/guard.txt";
    {
        std::ofstream stream(guard);
        assert(stream.good());
        stream << "preserve";
    }
    assert(symlink(guard.c_str(), path.c_str()) == 0);
    assert(!saveTouchLayoutFile(dir, profiles));
    assert(unlink(path.c_str()) == 0);
    const std::string staging = path + ".tmp";
    assert(symlink(guard.c_str(), staging.c_str()) == 0);
    assert(!saveTouchLayoutFile(dir, profiles));
    assert(unlink(staging.c_str()) == 0);
    {
        std::ifstream stream(guard);
        std::string original;
        assert(stream >> original);
        assert(original == "preserve");
    }
    assert(unlink(guard.c_str()) == 0);
    assert(rmdir(dir) == 0);
    puts("native touch JSON layout PASS");
}
