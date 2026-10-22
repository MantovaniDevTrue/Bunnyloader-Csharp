#include "graphics/FrameComposer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace bl::graphics {
namespace {

bool reject(std::string* error, const char* cause) {
    if (error) *error = cause;
    return false;
}

bool validSource(int value) {
    return value == kTexNone || value == kTexScreen || value == kTexHistory ||
           value <= -1000 || value > 0;
}

bool finiteDraw(const EffectDraw& draw) {
    if (!std::isfinite(draw.radius) || !std::isfinite(draw.intensity) ||
        !std::isfinite(draw.progress) || !std::isfinite(draw.age)) return false;
    for (float v : draw.box) if (!std::isfinite(v)) return false;
    for (float v : draw.center) if (!std::isfinite(v)) return false;
    for (float v : draw.color) if (!std::isfinite(v)) return false;
    for (const auto& uniform : draw.uniforms) {
        if (uniform.name.empty() || uniform.components < 1 || uniform.components > 4 ||
            uniform.v.empty() || uniform.v.size() > 256 ||
            uniform.v.size() % static_cast<size_t>(uniform.components) != 0) return false;
        for (float v : uniform.v) if (!std::isfinite(v)) return false;
    }
    return true;
}

}

bool appendNamespacedFrame(EffectFrame& destination, EffectFrame&& contribution,
                           const std::string& modId, std::string* error) {
    if (error) error->clear();
    if (modId.empty() || modId.size() > 128 || modId.find('/') != std::string::npos ||
        modId.find('\\') != std::string::npos || modId.find("..") != std::string::npos)
        return reject(error, "invalid mod identity");
    if (destination.targets.size() + contribution.targets.size() > FrameComposeLimits::MaxTargets ||
        destination.draws.size() + contribution.draws.size() > FrameComposeLimits::MaxDraws)
        return reject(error, "screen effect frame budget exceeded");

    int nextId = 1;
    std::unordered_set<int> occupied;
    for (const auto& target : destination.targets) {
        if (target.id > 0) {
            occupied.insert(target.id);
            if (target.id == std::numeric_limits<int>::max())
                return reject(error, "target id exhausted");
            nextId = std::max(nextId, target.id + 1);
        }
    }

    std::unordered_map<int, int> translate;
    for (const auto& target : contribution.targets) {
        if (target.id <= 0 || target.name.empty() || target.name.size() > 128 ||
            !std::isfinite(target.scale) || target.scale <= 0.f || target.scale > 1.f ||
            !translate.emplace(target.id, 0).second)
            return reject(error, "invalid or duplicate source target");
    }

    for (const auto& draw : contribution.draws) {
        if (draw.shader <= 0 || !finiteDraw(draw) || !validSource(draw.source) ||
            draw.dest < 0 || draw.uniforms.size() > 64 || draw.textures.size() > 6)
            return reject(error, "invalid effect draw");
        if ((draw.dest > 0 && !translate.contains(draw.dest)) ||
            (draw.source > 0 && !translate.contains(draw.source)))
            return reject(error, "unresolved render target reference");
        for (const auto& texture : draw.textures)
            if (texture.name.empty() || !validSource(texture.texture) ||
                (texture.texture > 0 && !translate.contains(texture.texture)))
                return reject(error, "invalid effect texture reference");
    }

    for (auto& target : contribution.targets) {
        if (nextId == std::numeric_limits<int>::max() && occupied.contains(nextId))
            return reject(error, "target id exhausted");
        while (occupied.contains(nextId)) {
            if (nextId == std::numeric_limits<int>::max())
                return reject(error, "target id exhausted");
            ++nextId;
        }
        const int newId = nextId;
        if (nextId < std::numeric_limits<int>::max()) ++nextId;
        occupied.insert(newId);
        translate.at(target.id) = newId;
        target.id = newId;
        target.name = modId + ":" + target.name;
    }

    for (auto& draw : contribution.draws) {
        if (draw.dest > 0) draw.dest = translate.at(draw.dest);
        if (draw.source > 0) draw.source = translate.at(draw.source);
        for (auto& texture : draw.textures)
            if (texture.texture > 0) texture.texture = translate.at(texture.texture);
    }

    destination.history = destination.history || contribution.history;
    destination.targets.reserve(destination.targets.size() + contribution.targets.size());
    destination.draws.reserve(destination.draws.size() + contribution.draws.size());
    for (auto& target : contribution.targets) destination.targets.push_back(std::move(target));
    for (auto& draw : contribution.draws) destination.draws.push_back(std::move(draw));
    return true;
}

}
