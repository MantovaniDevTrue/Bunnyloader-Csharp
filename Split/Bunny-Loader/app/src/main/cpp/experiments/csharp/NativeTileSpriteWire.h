#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

namespace bl::csharp::tile_sprite {

inline constexpr size_t kMaxSprites = 8;
inline constexpr size_t kMaxWireBytes = 1024;

struct Sprite {
    int type = -1;
    int x = 0;
    int y = 0;
    int frameX = 0;
    int frameY = 0;
    int width = 16;
    int height = 16;
    int offsetX = 0;
    int offsetY = 0;
    uint32_t argb = 0xffffffffu;
    int effects = 0;
    int textureLayer = 0;
    int phase = 0;
};

struct Batch {
    std::array<Sprite, kMaxSprites> sprites{};
    size_t count = 0;
};

inline bool valid(const Sprite& s) {
    return s.type >= 754 && s.type <= 65535 &&
           s.x >= 0 && s.x <= 100000 && s.y >= 0 && s.y <= 100000 &&
           s.frameX >= 0 && s.frameX <= 32767 &&
           s.frameY >= 0 && s.frameY <= 32767 &&
           s.width >= 1 && s.width <= 512 &&
           s.height >= 1 && s.height <= 512 &&
           s.frameX + s.width <= 32768 &&
           s.frameY + s.height <= 32768 &&
           s.offsetX >= -256 && s.offsetX <= 256 &&
           s.offsetY >= -256 && s.offsetY <= 256 &&
           s.effects >= 0 && s.effects <= 3 &&
           s.textureLayer >= 0 && s.textureLayer <= 2 &&
           s.phase >= 0 && s.phase <= 1;
}

inline bool integer(std::string_view text, int32_t* out) {
    if (text.empty()) return false;
    const char* first = text.data();
    const char* last = first + text.size();
    auto parsed = std::from_chars(first, last, *out);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

inline bool color(std::string_view text, uint32_t* out) {
    if (text.empty()) return false;
    const char* first = text.data();
    const char* last = first + text.size();
    auto parsed = std::from_chars(first, last, *out);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

inline bool parseSprite(std::string_view text, Sprite* output) {
    if (!output) return false;
    std::array<int32_t,9> n{};
    size_t from = 0;
    for (size_t i = 0; i < n.size(); ++i) {
        const size_t end = text.find(',', from);
        if (end == std::string_view::npos || !integer(text.substr(from, end-from), &n[i]))
            return false;
        from = end + 1;
    }
    uint32_t argb = 0;
    int32_t effects = 0,layer=0,phase=0;
    const size_t separator = text.find(',',from);
    if(separator == std::string_view::npos) {
        if(!color(text.substr(from),&argb))return false;
    } else {
        if(!color(text.substr(from,separator-from),&argb))return false;
        const size_t last=text.find(',',separator+1);
        if(last==std::string_view::npos) {
            if(!integer(text.substr(separator+1),&effects))return false;
        } else {
            if(!integer(text.substr(separator+1,last-separator-1),&effects))return false;
            const size_t phaseSep=text.find(',',last+1);
            if(phaseSep==std::string_view::npos) {
                if(!integer(text.substr(last+1),&layer))return false;
            } else {
                if(text.find(',',phaseSep+1)!=std::string_view::npos||
                   !integer(text.substr(last+1,phaseSep-last-1),&layer)||
                   !integer(text.substr(phaseSep+1),&phase))return false;
            }
        }
    }
    Sprite candidate{n[0],n[1],n[2],n[3],n[4],n[5],n[6],n[7],n[8],argb,effects,layer,phase};
    if (!valid(candidate)) return false;
    *output = candidate;
    return true;
}

inline bool parse(std::string_view text, Batch* output) {
    if (!output || text.size() > kMaxWireBytes ||
        !text.starts_with("SPRITES|")) return false;
    const size_t countEnd = text.find('|', 8);
    const std::string_view countText = countEnd == std::string_view::npos
        ? text.substr(8) : text.substr(8,countEnd-8);
    int32_t count = -1;
    if (!integer(countText,&count) || count < 0 ||
        count > static_cast<int32_t>(kMaxSprites)) return false;
    if (count == 0) {
        if (countEnd != std::string_view::npos) return false;
        *output = Batch{};
        return true;
    }
    if (countEnd == std::string_view::npos) return false;
    Batch candidate{};
    size_t from = countEnd + 1;
    for (int i = 0; i < count; ++i) {
        const size_t end = text.find('|',from);
        if ((i + 1 < count && end == std::string_view::npos) ||
            (i + 1 == count && end != std::string_view::npos)) return false;
        const size_t last = end == std::string_view::npos ? text.size() : end;
        if (from >= last ||
            !parseSprite(text.substr(from,last-from), &candidate.sprites[i]))
            return false;
        ++candidate.count;
        from = last+1;
    }
    *output = candidate;
    return true;
}

}
