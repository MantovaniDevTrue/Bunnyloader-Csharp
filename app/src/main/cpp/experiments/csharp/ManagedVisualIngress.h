#pragma once

#include "experiments/csharp/ManagedVisualQueue.h"
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace bl::csharp {

struct ManagedVisualWire {
    static constexpr unsigned Version = 1;
    static constexpr size_t MaxBytes = 512;

    static bool readInt(const std::string& input, int& value) {
        if (input.empty()) return false;
        char* end = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(input.c_str(), &end, 10);
        if (errno != 0 || end == input.c_str() || *end != '\0' ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }

    static bool readFloat(const std::string& input, float& value) {
        if (input.empty()) return false;
        char* end = nullptr;
        errno = 0;
        const float parsed = std::strtof(input.c_str(), &end);
        if (errno != 0 || end == input.c_str() || *end != '\0' ||
            !std::isfinite(parsed)) return false;
        value = parsed;
        return true;
    }

    static bool readColor(const std::string& input, uint32_t& value) {
        if (input.empty() || input[0] == '-') return false;
        char* end = nullptr;
        errno = 0;
        const unsigned long long parsed = std::strtoull(input.c_str(), &end, 10);
        if (errno != 0 || end == input.c_str() || *end != '\0' ||
            parsed > std::numeric_limits<uint32_t>::max()) return false;
        value = static_cast<uint32_t>(parsed);
        return true;
    }

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
        if (parts[0] != "V1") return false;
        int kind = 0;
        ManagedVisualCommand parsed;
        if (!readInt(parts[1], kind) || kind < 1 || kind > 4 ||
            !readInt(parts[2], parsed.playerIndex) ||
            !readInt(parts[3], parsed.effectId) ||
            !readFloat(parts[4], parsed.x) ||
            !readFloat(parts[5], parsed.y) ||
            !readFloat(parts[6], parsed.scale) ||
            !readFloat(parts[7], parsed.opacity) ||
            !readColor(parts[8], parsed.color) ||
            !readInt(parts[9], parsed.durationTicks)) return false;
        parsed.kind = static_cast<ManagedVisualKind>(kind);
        parsed.texture = parts[10];
        if (parsed.texture.find("..") != std::string::npos ||
            parsed.texture.find(static_cast<char>(92)) != std::string::npos ||
            parsed.texture.find('|') != std::string::npos) return false;
        output = std::move(parsed);
        return true;
    }

    static bool accept(ManagedVisualQueue& queue, const std::string& line) {
        ManagedVisualCommand cmd;
        return parse(line, cmd) && queue.push(std::move(cmd));
    }
};

}
