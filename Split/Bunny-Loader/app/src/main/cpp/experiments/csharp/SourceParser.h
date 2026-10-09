#pragma once

#include <map>
#include <string>
#include <vector>

namespace bl::csharp {

struct ParsedSourceType {
    std::string name;
    std::string base;
    std::map<std::string, std::string> properties;
};

bool parseCSharpSource(const std::string& source,
                       std::vector<ParsedSourceType>* definitions,
                       std::string* error,
                       bool requireContent = true);

}
