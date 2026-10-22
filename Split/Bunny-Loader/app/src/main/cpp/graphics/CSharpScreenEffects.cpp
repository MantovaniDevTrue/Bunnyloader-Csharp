#include "graphics/CSharpScreenEffects.h"
#include "graphics/EffectTypes.h"
#include "graphics/RenderHook.h"
#include "graphics/ShaderRegistry.h"
#include "experiments/csharp/SourceLoader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits.h>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>
#include <cstdlib>

namespace bl::graphics {
namespace {

struct Entry {
    std::string modId;
    std::string className;
    int shader = 0;
    int order = 0;
    int layer = 0;
    float intensity = 1.f;
};

std::mutex g_mutex;
std::vector<Entry> g_entries;

bool fail(std::string* error, const char* reason) {
    if (error) *error = reason;
    return false;
}

bool validRelativeShaderFile(const std::string& path) {
    if (path.size() < 6 || path.size() > 180 || path[0] == '/' ||
        path.find("..") != std::string::npos || path.find('\\') != std::string::npos ||
        path.substr(path.size() - 5) != ".frag") return false;
    for (unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '.') continue;
        return false;
    }
    return true;
}

bool readShader(const std::string& root, const std::string& relative, std::string* shader) {
    if (!validRelativeShaderFile(relative)) return false;
    char realRoot[PATH_MAX]{};
    char realFile[PATH_MAX]{};
    if (!realpath(root.c_str(), realRoot) ||
        !realpath((root + "/" + relative).c_str(), realFile)) return false;
    const std::string directory = std::string(realRoot) + "/";
    const std::string path(realFile);
    if (path.compare(0, directory.size(), directory) != 0) return false;
    struct stat metadata{};
    if (stat(path.c_str(), &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        metadata.st_size <= 0 || metadata.st_size > 65536) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    *shader = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !shader->empty() && shader->find("main(") != std::string::npos;
}

}

bool registerCSharpScreenEffects(const std::string& modId,
                                 const std::string& contentDirectory,
                                 const std::vector<CSharpScreenEffectSpec>& effects,
                                 std::string* error) {
    if (error) error->clear();
    if (modId.empty() || modId.size() > 128 || effects.empty() || effects.size() > 16)
        return fail(error, "screen effects: invalid count or mod identity");
    std::vector<std::pair<CSharpScreenEffectSpec, std::string>> sources;
    for (const auto& effect : effects) {
        if (effect.className.empty() || effect.className.size() > 100 ||
            effect.layer < 0 || effect.layer > 1 || effect.order < -4096 || effect.order > 4096 ||
            !std::isfinite(effect.intensity) || effect.intensity < 0.f || effect.intensity > 4.f)
            return fail(error, "screen effects: invalid class parameters");
        std::string shader;
        if (!readShader(contentDirectory, effect.shaderFile, &shader))
            return fail(error, "screen effects: invalid shader path or content");
        sources.push_back({effect, std::move(shader)});
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    for (const auto& entry : g_entries)
        if (entry.modId == modId)
            return fail(error, "screen effects: mod already registered");
    for (auto& pair : sources) {
        const auto& spec = pair.first;
        const int shader = ShaderRegistry::Register(modId + ":" + spec.className, pair.second);
        g_entries.push_back({modId, spec.className, shader, spec.order, spec.layer, spec.intensity});
    }
    std::stable_sort(g_entries.begin(), g_entries.end(), [](const Entry& a, const Entry& b) {
        if (a.modId != b.modId) return a.modId < b.modId;
        return a.order < b.order;
    });
    return true;
}

void pumpCSharpScreenEffects() {
    std::map<std::string, EffectFrame> byMod;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (const auto& entry : g_entries) {
            if (!bl::csharp::sourceRuntimeApplied(entry.modId)) continue;
            EffectDraw draw;
            draw.shader = entry.shader;
            draw.layer = entry.layer == 1 ? EffectLayer::Screen : EffectLayer::World;
            draw.source = kTexScreen;
            draw.intensity = entry.intensity;
            byMod[entry.modId].draws.push_back(std::move(draw));
        }
    }
    for (auto& [id, frame] : byMod)
        submitModEffects(id, std::move(frame));
}

}
