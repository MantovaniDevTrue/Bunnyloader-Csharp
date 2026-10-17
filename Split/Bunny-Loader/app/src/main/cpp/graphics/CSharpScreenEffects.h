#pragma once

#include <cstdint>
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

struct CSharpScreenSceneCallback {
    std::string nameSpace;
    std::string className;
    std::string methodName;
};
bool findCSharpScreenSceneCallback(const std::string& modId,
                                    CSharpScreenSceneCallback* result);
uint64_t csharpScreenWarmupRevision();
std::vector<TargetSpec> csharpScreenWarmupTargets();

bool registerCSharpScreenEffects(const std::string& modId,
                                 const std::string& contentDirectory,
                                 const std::vector<CSharpScreenEffectSpec>& effects,
                                 std::string* error);
void pumpCSharpScreenEffects();
void clearCSharpScreenEffects();

}
