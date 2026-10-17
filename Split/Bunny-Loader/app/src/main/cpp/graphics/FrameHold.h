#pragma once

#include "graphics/EffectTypes.h"

#include <map>
#include <string>
#include <utility>

namespace bl::graphics {

class FrameHold {
public:
    static constexpr unsigned MaxMissedWorldFrames = 4;

    void record(const std::string& modId, const EffectFrame& frame) {
        if (frame.empty()) {
            held_.erase(modId);
            return;
        }
        held_[modId] = Entry{frame, 0};
    }

    void remove(const std::string& modId) {
        held_.erase(modId);
    }

    unsigned replayMissing(std::map<std::string, EffectFrame>& contributions) {
        unsigned replays = 0;
        for (auto it = held_.begin(); it != held_.end();) {
            if (contributions.contains(it->first)) {
                it->second.missed = 0;
                ++it;
            } else if (it->second.missed < MaxMissedWorldFrames) {
                contributions.emplace(it->first, it->second.frame);
                ++it->second.missed;
                ++replays;
                ++it;
            } else {
                it = held_.erase(it);
            }
        }
        return replays;
    }

private:
    struct Entry {
        EffectFrame frame;
        unsigned missed = 0;
    };

    std::map<std::string, Entry> held_;
};

}
