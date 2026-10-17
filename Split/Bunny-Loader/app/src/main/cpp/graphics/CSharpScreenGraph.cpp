#include "graphics/CSharpScreenGraph.h"
#include "graphics/ShaderRegistry.h"
#include "graphics/TextureRegistry.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits.h>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <utility>

namespace bl::graphics {
namespace {

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

bool identifier(const std::string& value) {
    if (value.empty() || value.size() > 72) return false;
    for (const char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') continue;
        return false;
    }
    return true;
}

bool readAsset(const std::string& folder, const std::string& rel,
               const size_t limit, std::string* source) {
    if (rel.size() < 5 || rel.size() > 180 || rel[0] == '/' ||
        rel.find("..") != std::string::npos || rel.find('\\') != std::string::npos ||
        rel.find(':') != std::string::npos) return false;
    for (char c : rel) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '.') continue;
        return false;
    }
    char dir[PATH_MAX] = {}, file[PATH_MAX] = {};
    if (!realpath(folder.c_str(), dir) || !realpath((folder + "/" + rel).c_str(), file))
        return false;
    const std::string root = std::string(dir) + "/";
    if (std::string(file).compare(0, root.size(), root) != 0) return false;
    struct stat st{};
    if (stat(file, &st) || !S_ISREG(st.st_mode) ||
        st.st_size <= 0 || static_cast<size_t>(st.st_size) > limit) return false;
    std::ifstream input(file, std::ios::binary);
    if (!input) return false;
    *source = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return source->size() <= limit && !source->empty();
}

std::vector<std::string> split(const std::string& text, char delim) {
    std::vector<std::string> result;
    size_t at = 0;
    while (at <= text.size()) {
        const size_t end = text.find(delim, at);
        result.push_back(text.substr(at, end == std::string::npos ? end : end - at));
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return result;
}

bool scalar(const std::string& str, float* value) {
    if (str.empty() || str.size() > 24) return false;
    errno = 0;
    char* end = nullptr;
    *value = std::strtof(str.c_str(), &end);
    return errno == 0 && end && !*end && std::isfinite(*value);
}

bool reference(const std::string& value, const std::map<std::string,int>& targets,
               const bool isSource, int* id) {
    if (value == "screen") { *id = isSource ? kTexScreen : 0; return true; }
    if (value == "history" && isSource) { *id = kTexHistory; return true; }
    const auto it = targets.find(value);
    if (it == targets.end()) return false;
    *id = it->second;
    return true;
}

bool tokens(const std::string& list, std::vector<std::pair<std::string,std::string>>* out) {
    if (list == "-" || list.empty()) return true;
    const auto fields = split(list, ',');
    if (fields.size() > 16) return false;
    std::set<std::string> names;
    for (const auto& field : fields) {
        const size_t pos = field.find('=');
        if (pos == std::string::npos) return false;
        const std::string name = field.substr(0, pos), value = field.substr(pos+1);
        if (!identifier(name) || value.empty() || value.size() > 180 ||
            !names.emplace(name).second) return false;
        out->emplace_back(name,value);
    }
    return true;
}

bool validUniform(const std::string& value) {
    if (value == "@sun" || value == "@clock" || value == "@day" ||
        value == "@strength" || value == "@threshold" ||
        value == "@enabled") return true;
    float v = 0.f;
    return scalar(value, &v) && v >= -100.f && v <= 100.f;
}

bool pngPath(const std::string& path) {
    return path.size() >= 5 && path.substr(path.size() - 4) == ".png";
}

}

bool loadCSharpScreenGraph(const std::string& modId, const std::string& className,
                           const std::string& folder, const std::string& relative,
                           EffectLayer layer, float intensity, ScreenGraph* graph,
                           std::string* error) {
    if (!graph || modId.empty() || !identifier(className) ||
        !std::isfinite(intensity) || intensity < 0.f || intensity > 4.f)
        return fail(error, "screen graph: invalid identity or intensity");
    if (relative.size() < 9 || relative.substr(relative.size()-9) != ".screenfx")
        return fail(error, "screen graph: .screenfx file required");
    std::string source;
    if (!readAsset(folder, relative, 16384, &source))
        return fail(error, "screen graph: missing or invalid file");
    ScreenGraph pending;
    pending.layer = layer;
    pending.intensity = intensity;
    std::map<std::string,int> targets;
    std::vector<std::vector<std::string>> passes;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream,line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto cells = split(line, '|');
        if (cells.size() == 3 && cells[0] == "TARGET") {
            float scale = 0.f;
            if (targets.size() >= 8 || !identifier(cells[1]) ||
                !scalar(cells[2], &scale) || scale < .25f || scale > 1.f ||
                targets.contains(cells[1]))
                return fail(error, "screen graph: invalid target");
            int id = static_cast<int>(targets.size()) + 1;
            targets.emplace(cells[1], id);
            pending.targets.push_back({id, cells[1], scale, false});
        } else if (cells.size() == 6 && cells[0] == "PASS") {
            if (passes.size() >= 16) return fail(error, "screen graph: too many passes");
            passes.push_back(std::move(cells));
        } else {
            return fail(error, "screen graph: invalid directive");
        }
    }
    if (passes.empty()) return fail(error, "screen graph: missing passes");
    for (size_t index = 0; index < passes.size(); ++index) {
        const auto& cells = passes[index];
        if (cells[1].size() < 6 || cells[1].substr(cells[1].size()-5) != ".frag")
            return fail(error, "screen graph: pass shader must be .frag");
        std::string shaderSource;
        if (!readAsset(folder, cells[1], 65536, &shaderSource) ||
            shaderSource.find("main(") == std::string::npos)
            return fail(error, "screen graph: invalid shader");
        ScreenGraphPass pass;
        if (!reference(cells[2], targets, true, &pass.source) ||
            !reference(cells[3], targets, false, &pass.dest) ||
            (pass.source > 0 && pass.source == pass.dest))
            return fail(error, "screen graph: unresolved or feedback target");
        std::vector<std::pair<std::string,std::string>> uniforms, textures;
        if (!tokens(cells[4], &uniforms) || !tokens(cells[5], &textures) ||
            textures.size() > 6)
            return fail(error, "screen graph: invalid uniforms or textures");
        for (const auto& [name, value] : uniforms) {
            if (!validUniform(value)) return fail(error, "screen graph: invalid uniform value");
            pass.uniforms.push_back({name,value});
        }
        for (const auto& [name,value] : textures) {
            ScreenGraphTexture binding;
            binding.name = name;
            if (value == "screen" || value == "history" || targets.contains(value)) {
                if (!reference(value, targets, true, &binding.day))
                    return fail(error, "screen graph: invalid texture target");
                binding.night = binding.day;
                if (binding.day == pass.dest)
                    return fail(error, "screen graph: feedback texture");
            } else {
                const auto pair = split(value, '?');
                if (pair.size() > 2 || pair.empty() ||
                    !pngPath(pair[0]) || (pair.size()==2 && !pngPath(pair[1])))
                    return fail(error, "screen graph: invalid texture path");
                const std::string names[2] = {pair[0],pair.size()==2?pair[1]:pair[0]};
                int ids[2] = {};
                for (int i=0;i<2;i++) {
                    std::string data;
                    if (!readAsset(folder, names[i], 1048576, &data))
                        return fail(error, "screen graph: missing texture asset");
                    int w=0,h=0;
                    ids[i] = TextureRegistry::Load(folder + "/" + names[i], true, false, &w, &h);
                    if (ids[i] > -1000 || w < 1 || h < 1 || w > 2048 || h > 2048)
                        return fail(error, "screen graph: invalid image texture");
                }
                binding.day=ids[0];
                binding.night=ids[1];
            }
            pass.textures.push_back(binding);
        }
        pass.shader = ShaderRegistry::Register(modId + ":" + className + ":" + std::to_string(index),
                                               shaderSource);
        if (pass.shader <= 0) return fail(error, "screen graph: shader registration failed");
        pending.passes.push_back(std::move(pass));
    }
    *graph = std::move(pending);
    return true;
}

void buildCSharpScreenGraph(const ScreenGraph& graph, const ScreenSceneSnapshot& scene,
                           EffectFrame* frame) {
    if (!frame || !scene.valid) return;
    for (const auto& t : graph.targets) frame->targets.push_back(t);
    for (const auto& pass : graph.passes) {
        EffectDraw draw;
        draw.shader = pass.shader;
        draw.layer = graph.layer;
        draw.source = pass.source;
        draw.dest = pass.dest;
        draw.intensity = graph.intensity;
        for (const auto& uniform : pass.uniforms) {
            UniformValue value;
            value.name = uniform.name;
            const auto& expr = uniform.value;
            if (expr == "@sun") {
                value.components = 2;
                value.v = {scene.sunX, scene.sunY};
            } else if (expr == "@clock") value.v = {scene.clock};
            else if (expr == "@day") value.v = {scene.daytime ? 1.f : 0.f};
            else if (expr == "@strength") value.v = {scene.strength * graph.intensity};
            else if (expr == "@threshold") value.v = {scene.shadowThreshold};
            else if (expr == "@enabled") value.v = {scene.sunEnabled ? 1.f : 0.f};
            else {
                float constant = 0.f;
                if (!scalar(expr,&constant)) continue;
                value.v = {constant};
            }
            draw.uniforms.push_back(std::move(value));
        }
        for (const auto& texture : pass.textures)
            draw.textures.push_back({texture.name,scene.daytime ? texture.day : texture.night});
        frame->draws.push_back(std::move(draw));
    }
}

}
