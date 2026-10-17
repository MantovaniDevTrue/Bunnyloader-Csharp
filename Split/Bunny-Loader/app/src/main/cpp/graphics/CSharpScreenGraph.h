#pragma once
#include "graphics/EffectTypes.h"
#include "graphics/ScreenScene.h"

#include <string>
#include <vector>

namespace bl::graphics {

struct ScreenGraphUniform {
    std::string name;
    std::string value;
};

struct ScreenGraphTexture {
    std::string name;
    int day = 0;
    int night = 0;
};

struct ScreenGraphPass {
    int shader = 0;
    int source = kTexScreen;
    int dest = 0;
    std::vector<ScreenGraphUniform> uniforms;
    std::vector<ScreenGraphTexture> textures;
};

struct ScreenGraph {
    std::vector<TargetSpec> targets;
    std::vector<ScreenGraphPass> passes;
    EffectLayer layer = EffectLayer::World;
    float intensity = 1.f;
};

bool loadCSharpScreenGraph(const std::string& modId, const std::string& className,
                           const std::string& contentDirectory, const std::string& relativePath,
                           EffectLayer layer, float intensity, ScreenGraph* graph,
                           std::string* error);
void buildCSharpScreenGraph(const ScreenGraph& graph, const ScreenSceneSnapshot& scene,
                           EffectFrame* frame);

}
