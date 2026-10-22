#include "graphics/CSharpScreenEffects.h"
#include "graphics/RenderHook.h"
#include "graphics/ShaderRegistry.h"

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
bool sourceRuntimeApplied(const std::string& id) { return id == "probe"; }
}

namespace bl::graphics {
int ShaderRegistry::Register(const std::string&, const std::string&) { return ++shaders; }
bool submitModEffects(const std::string&, EffectFrame&& frame) {
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
    bl::graphics::CSharpScreenEffectSpec spec{"Tint", "Effects/Ok.frag", 0, 0, 0.7f};
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
    fs::remove_all(path);
    return 0;
}
