#include "graphics/CSharpScreenEffects.h"
#include "graphics/RenderHook.h"
#include "graphics/ShaderRegistry.h"
#include "graphics/ScreenScene.h"
#include "graphics/TextureRegistry.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;
static std::vector<bl::graphics::EffectFrame> frames;
static int shaders = 0;

namespace bl::csharp {
bool sourceRuntimeApplied(const std::string& id) { return id == "probe" || id == "probe_graph"; }
}

namespace bl::graphics {
int ShaderRegistry::Register(const std::string&, const std::string&) { return ++shaders; }
int TextureRegistry::Load(const std::string&, bool, bool, int* w, int* h) {
    if (w) *w = 4;
    if (h) *h = 1;
    static int number = -1000;
    return number--;
}
ScreenSceneSnapshot sampleScreenScene() {
    ScreenSceneSnapshot scene;
    scene.valid = true;
    scene.daytime = true;
    scene.sunX = .4f;
    scene.sunY = .8f;
    return scene;
}
bool submitModEffects(const std::string&, EffectFrame&& frame, bool) {
    frames.push_back(std::move(frame));
    return true;
}
}

int main() {
    char temp[] = "/tmp/screenfx-registry-XXXXXX";
    const char* path = mkdtemp(temp);
    assert(path);
    fs::create_directories(fs::path(path) / "Effects");
    {
        std::ofstream file(fs::path(path) / "Effects" / "Ok.frag");
        file << "void main(){fragColor=vec4(1.0);}";
    }
    bl::graphics::CSharpScreenEffectSpec spec{"Tint", "Effects/Ok.frag", 0, 0, 0.7f, ""};
    std::string error;
    assert(bl::graphics::registerCSharpScreenEffects("probe", path, {spec}, &error));
    assert(shaders == 1);
    bl::graphics::pumpCSharpScreenEffects();
    assert(frames.size() == 1);
    assert(frames[0].draws.size() == 1);
    assert(frames[0].draws[0].intensity == 0.7f);
    assert(frames[0].draws[0].source == bl::graphics::kTexScreen);
    spec.shaderFile = "../outside.frag";
    assert(!bl::graphics::registerCSharpScreenEffects("attack", path, {spec}, &error));
    spec.shaderFile = "Effects/Ok.frag";
    assert(!bl::graphics::registerCSharpScreenEffects("probe", path, {spec}, &error));
    assert(shaders == 1);
    {
        std::ofstream file(fs::path(path) / "Effects" / "Light.screenfx");
        file << "TARGET|shadow|0.333333\n"
                "PASS|Effects/Ok.frag|screen|shadow|uSun=@sun,uShadowThreshold=@threshold|-\n"
                "PASS|Effects/Ok.frag|screen|screen|uClock=@clock|uRays=shadow,uGradient=Effects/Day.png?Effects/Night.png\n";
    }
    {
        std::ofstream file(fs::path(path) / "Effects" / "Day.png", std::ios::binary);
        file << "png mock";
    }
    {
        std::ofstream file(fs::path(path) / "Effects" / "Night.png", std::ios::binary);
        file << "png mock";
    }
    bl::graphics::CSharpScreenEffectSpec graph;
    graph.className = "Light";
    graph.pipelineFile = "Effects/Light.screenfx";
    assert(bl::graphics::registerCSharpScreenEffects("probe_graph", path, {graph}, &error));
    assert(shaders == 3);
    bl::graphics::pumpCSharpScreenEffects();
    assert(frames.size() == 3);
    assert(frames[2].targets.size() == 1);
    assert(frames[2].draws.size() == 2);
    assert(frames[2].draws[0].dest == 1);
    assert(frames[2].draws[1].textures.size() == 2);
    assert(frames[2].draws[1].textures[0].texture == 1);
    assert(frames[2].draws[1].textures[1].texture <= -1000);
    graph.pipelineFile = "../Light.screenfx";
    assert(!bl::graphics::registerCSharpScreenEffects("bad_graph", path, {graph}, &error));
    fs::remove_all(path);
    return 0;
}
