#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::localization {
struct Entry {
    std::string culture;
    std::string key;
    std::string value;
};
bool decodeWire(const std::vector<std::string>& fields, const std::string& modId,
                Entry* result, std::string* error);
bool decodeManagedWire(const std::string& wire, const std::string& modId,
                       std::vector<Entry>* result, std::string* error);
bool acceptManagedWire(const std::string& modId, const std::string& assemblyPath,
                       const std::string& culture, uint64_t generation,
                       const std::string& wire);
void retireMod(const std::string& modId);
void stage(const std::string& modId, std::vector<Entry> entries);
void bindManagedAssembly(const std::string& modId, const std::string& assemblyPath);
void retainActiveMods(const std::set<std::string>& activeIds);
void pumpOnGameThread();
}
