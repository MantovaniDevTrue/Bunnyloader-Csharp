#pragma once
#include <string>
namespace bl::runtime::backgrounds {
// Reserve on the game thread while content loads. Identity is mod UID + path;
// slots are process-local and must never be serialized into worlds.
int reserve(const std::string& mod, const std::string& path,
            const std::string& absoluteFile, std::string* error);
bool autoload(const std::string& mod, const std::string& contentDirectory, std::string* error);
int totalSlots();
bool pending();
int find(const std::string& mod, const std::string& path, bool requireReady = true);
bool publish(std::string* error, const std::string& modFilter = "");
// 1 ready (or no assets), 0 queued, -1 failed with original diagnostic.
int readiness(const std::string& mod, std::string* error);
void unload(const std::string& mod);
int count();
}
extern "C" int bunny_background_texture_slot(const char* mod, const char* path);
