#include "graphics/CSharpScreenEffects.h"
#include "core/Log.h"
#include "graphics/CSharpScreenGraph.h"
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
    bool isGraph = false;
    ScreenGraph graph;
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
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (const auto& entry : g_entries)
            if (entry.modId == modId)
                return fail(error, "screen effects: mod already registered");
    }
    std::vector<Entry> additions;
    for (const auto& effect : effects) {
        if (effect.className.empty() || effect.className.size() > 100 ||
            effect.layer < 0 || effect.layer > 1 || effect.order < -4096 || effect.order > 4096 ||
            !std::isfinite(effect.intensity) || effect.intensity < 0.f || effect.intensity > 4.f ||
            (effect.shaderFile.empty() == effect.pipelineFile.empty()))
            return fail(error, "screen effects: invalid class parameters");
        Entry entry;
        entry.modId = modId;
        entry.className = effect.className;
        entry.order = effect.order;
        entry.layer = effect.layer;
        entry.intensity = effect.intensity;
        entry.isGraph = !effect.pipelineFile.empty();
        if (entry.isGraph) {
            if (!loadCSharpScreenGraph(modId, effect.className, contentDirectory,
                effect.pipelineFile, effect.layer == 1 ? EffectLayer::Screen : EffectLayer::World,
                effect.intensity, &entry.graph, error)) return false;
        } else {
            std::string shader;
            if (!readShader(contentDirectory, effect.shaderFile, &shader))
                return fail(error, "screen effects: invalid shader path or content");
            entry.shader = ShaderRegistry::Register(modId + ":" + effect.className, shader);
            if (entry.shader <= 0)
                return fail(error, "screen effects: shader registration failed");
        }
        additions.push_back(std::move(entry));
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    for (const auto& entry : g_entries)
        if (entry.modId == modId)
            return fail(error, "screen effects: mod already registered");
    int graphPasses = 0, graphTargets = 0;
    for (const auto& entry : additions) {
        if (!entry.isGraph) continue;
        graphPasses += static_cast<int>(entry.graph.passes.size());
        graphTargets += static_cast<int>(entry.graph.targets.size());
    }
    for (auto& entry : additions) g_entries.push_back(std::move(entry));
    if (graphPasses)
        BL_INFO("CSharp ScreenFx: grafo registrado mod=%s passes=%d alvos=%d",
                modId.c_str(), graphPasses, graphTargets);
    std::stable_sort(g_entries.begin(), g_entries.end(), [](const Entry& a, const Entry& b) {
        if (a.modId != b.modId) return a.modId < b.modId;
        return a.order < b.order;
    });
    return true;
}


void clearCSharpScreenEffects() {
    std::lock_guard<std::mutex> guard(g_mutex);
    std::string previous;
    for (const auto& entry : g_entries) {
        if (entry.modId == previous) continue;
        previous = entry.modId;
        submitModEffects(previous, EffectFrame{}, true);
    }
}

void pumpCSharpScreenEffects() {
    std::map<std::string, EffectFrame> byMod;
    const ScreenSceneSnapshot scene = sampleScreenScene();
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (const auto& entry : g_entries) {
            if (!bl::csharp::sourceRuntimeApplied(entry.modId)) continue;
            if (entry.isGraph) {
                if (!scene.valid) continue;
                auto& frame = byMod[entry.modId];
                const int offset = static_cast<int>(frame.targets.size()) * 16;
                EffectFrame next;
                buildCSharpScreenGraph(entry.graph, scene, &next);
                for (auto& target : next.targets) target.id += offset;
                for (auto& draw : next.draws) {
                    if (draw.source > 0) draw.source += offset;
                    if (draw.dest > 0) draw.dest += offset;
                    for (auto& tex : draw.textures)
                        if (tex.texture > 0) tex.texture += offset;
                    frame.draws.push_back(std::move(draw));
                }
                for (auto& target : next.targets) frame.targets.push_back(std::move(target));
                continue;
            }
            EffectDraw draw;
            draw.shader = entry.shader;
            draw.layer = entry.layer == 1 ? EffectLayer::Screen : EffectLayer::World;
            draw.source = kTexScreen;
            draw.intensity = entry.intensity;
            byMod[entry.modId].draws.push_back(std::move(draw));
        }
    }
    static bool loggedFirstGraphFrame = false;
    for (auto& [id, frame] : byMod) {
        if (!loggedFirstGraphFrame && !frame.targets.empty()) {
            BL_INFO("CSharp ScreenFx: primeira entrega mod=%s passes=%zu alvos=%zu cena=%d dia=%d luz=%.2f",
                    id.c_str(), frame.draws.size(), frame.targets.size(),
                    scene.valid, scene.daytime, scene.strength);
            loggedFirstGraphFrame = true;
        }
        if (!submitModEffects(id, std::move(frame), true))
            BL_WARN("CSharp ScreenFx: entrega recusada mod=%s", id.c_str());
    }
}

}
