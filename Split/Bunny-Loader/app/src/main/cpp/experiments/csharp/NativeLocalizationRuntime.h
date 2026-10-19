#pragma once
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
void stage(const std::string& modId, std::vector<Entry> entries);
void pumpOnGameThread();
}
