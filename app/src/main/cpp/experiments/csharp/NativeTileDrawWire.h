#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace bl::csharp::tile_draw {

struct Data {
    int32_t frameX = 0;
    int32_t frameY = 0;
    int32_t width = 16;
    int32_t height = 16;
    int32_t top = 0;
    int32_t addFrameX = 0;
    int32_t addFrameY = 0;
    int32_t effects = 0;
    bool drawDefault = true;
};

inline bool valid(const Data& d) {
    return d.frameX >= 0 && d.frameX <= 32767 &&
           d.frameY >= 0 && d.frameY <= 32767 &&
           d.width >= 1 && d.width <= 512 &&
           d.height >= 1 && d.height <= 512 &&
           d.top >= -128 && d.top <= 128 &&
           d.addFrameX >= -8192 && d.addFrameX <= 8192 &&
           d.addFrameY >= -8192 && d.addFrameY <= 8192 &&
           d.frameX + d.addFrameX >= 0 &&
           d.frameY + d.addFrameY >= 0 &&
           d.effects >= 0 && d.effects <= 3;
}

inline bool parse(std::string_view reply, Data* output) {
    if (!output || reply.size() > 160 || reply.substr(0, 5) != "DRAW|") return false;
    std::array<int32_t, 8> values{};
    size_t begin = 5;
    bool drawDefault = true;
    for (size_t index = 0; index < values.size(); ++index) {
        const size_t separator = reply.find('|', begin);
        if (index + 1 < values.size() && separator == std::string_view::npos)
            return false;
        if (index + 1 == values.size() && separator != std::string_view::npos) {
            const std::string_view visibility = reply.substr(separator + 1);
            if (visibility != "0" && visibility != "1") return false;
            drawDefault = visibility == "1";
        }
        const size_t end = separator == std::string_view::npos ? reply.size() : separator;
        if (begin >= end) return false;
        const char* first = reply.data() + begin;
        const char* last = reply.data() + end;
        const auto parsed = std::from_chars(first, last, values[index]);
        if (parsed.ec != std::errc{} || parsed.ptr != last) return false;
        begin = end + 1;
    }
    const Data candidate{values[0], values[1], values[2], values[3],
                         values[4], values[5], values[6], values[7], drawDefault};
    if (!valid(candidate)) return false;
    *output = candidate;
    return true;
}

}
