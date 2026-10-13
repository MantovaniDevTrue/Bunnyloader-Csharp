#pragma once

#include "experiments/csharp/ManagedVisualQueue.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace bl::csharp {

struct ManagedVisualWire {
    static constexpr unsigned Version = 1;
    static constexpr size_t MaxBytes = 512;

    static bool parse(const std::string& line, ManagedVisualCommand& output) {
        if (line.empty() || line.size() > MaxBytes || line.find('\n') != std::string::npos)
            return false;
        size_t indexes[10] = {};
        size_t fields = 0;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '|') {
                if (fields >= 10) return false;
                indexes[fields++] = i;
            }
        }
        if (fields != 10) return false;
        std::string parts[11];
        size_t start = 0;
        for (size_t i = 0; i < 10; ++i) {
            parts[i] = line.substr(start, indexes[i] - start);
            start = indexes[i] + 1;
        }
        parts[10] = line.substr(start);
        try {
            if (parts[0] != "V1") return false;
            auto kind = std::stoi(parts[1]);
            if (kind < 1 || kind > 4) return false;
            ManagedVisualCommand parsed;
            parsed.kind = static_cast<ManagedVisualKind>(kind);
            parsed.playerIndex = std::stoi(parts[2]);
            parsed.effectId = std::stoi(parts[3]);
            parsed.x = std::stof(parts[4]);
            parsed.y = std::stof(parts[5]);
            parsed.scale = std::stof(parts[6]);
            parsed.opacity = std::stof(parts[7]);
            parsed.color = static_cast<uint32_t>(std::stoul(parts[8]));
            parsed.durationTicks = std::stoi(parts[9]);
            parsed.texture = parts[10];
            if (parsed.texture.find("..") != std::string::npos ||
                parsed.texture.find('\\') != std::string::npos ||
                parsed.texture.find('|') != std::string::npos) return false;
            output = std::move(parsed);
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool accept(ManagedVisualQueue& queue, const std::string& line) {
        ManagedVisualCommand cmd;
        return parse(line, cmd) && queue.push(std::move(cmd));
    }
};

}
