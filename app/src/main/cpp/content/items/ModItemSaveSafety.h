#pragma once

#include "content/items/ModItemSaveFormat.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace bl::runtime::item_save {

inline bool canReplaceExisting(const std::string& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) return false;
    if (!exists) return true;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<SavedItem> verified;
    return read(input, verified, true);
}

inline bool preserveLegacyBeforeRaritySave(const std::string& path,
    const std::vector<SavedItem>& items) {
    if (std::none_of(items.begin(), items.end(),
        [](const SavedItem& item) { return !item.rarityKey.empty(); })) return true;
    std::ifstream source(path, std::ios::binary);
    if (!source) {
        std::error_code error;
        return !std::filesystem::exists(path, error) && !error;
    }
    std::string header;
    if (!std::getline(source, header)) return false;
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header == "bunnyloader itens 3") {
        source.clear();
        source.seekg(0);
        std::vector<SavedItem> verified;
        return read(source, verified, true);
    }
    if (header != "bunnyloader itens 1" && header != "bunnyloader itens 2") return false;
    const std::string backup = path + ".before-rarity-v3.bak";
    std::error_code error;
    const bool backupExists = std::filesystem::exists(backup, error);
    if (error) return false;
    if (backupExists) {
        std::ifstream preserved(backup, std::ios::binary);
        if (!preserved) return false;
        std::vector<SavedItem> verified;
        if (!read(preserved, verified, true)) return false;
        std::string backupHeader;
        preserved.clear();
        preserved.seekg(0);
        if (!std::getline(preserved, backupHeader)) return false;
        if (!backupHeader.empty() && backupHeader.back() == '\r')
            backupHeader.pop_back();
        return backupHeader == "bunnyloader itens 1" ||
            backupHeader == "bunnyloader itens 2";
    }
    const std::string temp = backup + ".tmp";
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    source.clear();
    source.seekg(0);
    output << source.rdbuf();
    output.flush();
    const bool good = output.good() && !source.bad();
    output.close();
    if (!good || std::rename(temp.c_str(), backup.c_str()) != 0) {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

}
