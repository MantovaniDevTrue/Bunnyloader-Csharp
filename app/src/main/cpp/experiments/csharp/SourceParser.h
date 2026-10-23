#pragma once

#include <map>
#include <string>
#include <vector>

namespace bl::csharp {

struct ParsedSourceType {
    std::string name;
    std::string base;
    std::string namespaceName;
    std::map<std::string, std::string> properties;
    std::vector<std::pair<std::string,std::string>> localizedNames{};
};

bool parseCSharpSource(const std::string& source,
                       std::vector<ParsedSourceType>* definitions,
                       std::string* error,
                       bool requireContent = true,
                       int* localizationModules = nullptr);

}
