#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::touch {
struct SavedSlot {
    std::string slot;
    int index = 0;
    std::string action;
    std::vector<std::string> group;
};
using SavedProfiles = std::map<std::string, std::vector<SavedSlot>>;

// Mesmo esquema BunnyTouchLayoutStore.cs, sem JavaScript no loader C#.
// String vazia em memória é serializada como JSON null para botões ausentes.
bool readTouchLayoutJson(std::string_view json, SavedProfiles* data);
std::string writeTouchLayoutJson(const SavedProfiles& data);
bool loadTouchLayoutFile(const std::string& saveDir, SavedProfiles* data);
bool saveTouchLayoutFile(const std::string& saveDir, const SavedProfiles& data);
} // namespace bl::csharp::touch
