#pragma once
#include <charconv>
#include <cstdint>
#include <string_view>

namespace bl::csharp {

inline bool decodeRarityColorWire(std::string_view text, uint32_t* argb) {
    if (!argb || !text.starts_with("COLOR|") || text.size() > 32) return false;
    uint8_t channels[4]{};
    text.remove_prefix(6);
    for (int i = 0; i < 4; ++i) {
        const auto split = text.find('|');
        if ((i < 3 && split == std::string_view::npos) ||
            (i == 3 && split != std::string_view::npos)) return false;
        const auto token = i == 3 ? text : text.substr(0, split);
        if (token.empty()) return false;
        int value = -1;
        auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            value < 0 || value > 255) return false;
        channels[i] = static_cast<uint8_t>(value);
        if (i < 3) text.remove_prefix(split + 1);
    }
    *argb = (static_cast<uint32_t>(channels[3]) << 24) |
        (static_cast<uint32_t>(channels[0]) << 16) |
        (static_cast<uint32_t>(channels[1]) << 8) |
        static_cast<uint32_t>(channels[2]);
    return true;
}


inline bool decodeRarityPrefixWire(std::string_view text, int* result) {
    if (!result || !text.starts_with("RARITY|") || text.size() > 18) return false;
    text.remove_prefix(7);
    if (text.empty()) return false;
    int value = -100;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        value < -11 || value >= 4108) return false;
    *result = value;
    return true;
}

}
