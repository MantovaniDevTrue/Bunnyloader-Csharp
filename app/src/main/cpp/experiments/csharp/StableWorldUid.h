#pragma once
#include "experiments/csharp/PortableSha256.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace bl::csharp::worldsave {

inline std::string stableWorldSystemUid(std::string_view sourceModUid) {
    if (sourceModUid.empty() || sourceModUid.size() > 128) return {};
    for (unsigned char ch : sourceModUid)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'))
            return {};
    std::string uid(sourceModUid);
    if (uid.size() == 36) {
        bool guid = true;
        for (size_t i = 0; i < uid.size(); ++i) {
            const unsigned char ch = static_cast<unsigned char>(uid[i]);
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                if (ch != '-') guid = false;
            } else if (!std::isxdigit(ch)) guid = false;
        }
        if (guid) {
            std::transform(uid.begin(), uid.end(), uid.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return uid;
        }
    }
    PortableSha256 sha;
    sha.field("BunnyLoader.WorldSystemUID.v1");
    sha.field(sourceModUid);
    std::string hex = sha.finish().substr(0, 32);
    hex[12] = '8';
    const char variants[] = "89ab";
    const char value = hex[16];
    const unsigned nibble = value >= 'a' ? static_cast<unsigned>(value - 'a' + 10)
                                        : static_cast<unsigned>(value - '0');
    hex[16] = variants[nibble & 3];
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" +
           hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" +
           hex.substr(20, 12);
}
}
