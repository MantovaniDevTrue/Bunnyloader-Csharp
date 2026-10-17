#include "graphics/FrameHold.h"

#include <cassert>
#include <map>
#include <string>

int main() {
    using namespace bl::graphics;
    FrameHold cache;
    EffectFrame base;
    base.targets.push_back({1, "shadow", .333333f, false});
    EffectDraw draw;
    draw.shader = 42;
    draw.source = kTexScreen;
    draw.dest = 1;
    base.draws.push_back(draw);
    cache.record("lights", base);
    for (unsigned i = 0; i < FrameHold::MaxMissedWorldFrames; ++i) {
        std::map<std::string, EffectFrame> contributions;
        assert(cache.replayMissing(contributions) == 1);
        assert(contributions.at("lights").draws.size() == 1);
        assert(contributions.at("lights").targets.size() == 1);
        assert(contributions.at("lights").draws[0].shader == 42);
    }
    {
        std::map<std::string, EffectFrame> contributions;
        assert(cache.replayMissing(contributions) == 0);
        assert(contributions.empty());
    }
    cache.record("lights", base);
    {
        std::map<std::string, EffectFrame> updates;
        updates.emplace("lights", base);
        assert(cache.replayMissing(updates) == 0);
    }
    {
        std::map<std::string, EffectFrame> contributions;
        assert(cache.replayMissing(contributions) == 1);
    }
    cache.remove("lights");
    {
        std::map<std::string, EffectFrame> contributions;
        assert(cache.replayMissing(contributions) == 0);
    }
    cache.record("lights", base);
    cache.record("lights", EffectFrame{});
    {
        std::map<std::string, EffectFrame> contributions;
        assert(cache.replayMissing(contributions) == 0);
    }
    return 0;
}
