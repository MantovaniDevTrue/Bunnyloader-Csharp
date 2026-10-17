#pragma once

#include <string>
#include <vector>

namespace bl::graphics {

struct CSharpScreenEffectSpec {
    std::string className;
    std::string shaderFile;
    int layer = 0;
    int order = 0;
    float intensity = 1.f;
};

bool registerCSharpScreenEffects(const std::string& modId,
                                 const std::string& contentDirectory,
                                 const std::vector<CSharpScreenEffectSpec>& effects,
                                 std::string* error);
void pumpCSharpScreenEffects();

}
