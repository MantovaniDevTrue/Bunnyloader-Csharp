#include "experiments/csharp/NativeTouchLayoutJson.h"
#include <cassert>
#include <cstdio>
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
    assert(rmdir(dir) == 0);
    puts("native touch JSON layout PASS");
}
