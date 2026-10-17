#pragma once

#include <map>
#include <string>
#include <vector>
#include "graphics/EffectTypes.h"

namespace bl::graphics {

struct CSharpScreenEffectSpec {
    std::string className;
    std::string shaderFile;
    int layer = 0;
    int order = 0;
    float intensity = 1.f;
    std::string pipelineFile;
    std::map<std::string, float> parameters;
};

std::vector<TargetSpec> csharpScreenWarmupTargets();

bool registerCSharpScreenEffects(const std::string& modId,
                                 const std::string& contentDirectory,
                                 const std::vector<CSharpScreenEffectSpec>& effects,
                                 std::string* error);
void pumpCSharpScreenEffects();
void clearCSharpScreenEffects();

}
