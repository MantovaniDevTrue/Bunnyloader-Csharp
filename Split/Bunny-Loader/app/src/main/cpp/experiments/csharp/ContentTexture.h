#pragma once
#include <string>

namespace bl::csharp {
// Shared by source registration and contract tests. Only relative Content PNGs.
inline bool normalizeContentTexture(const char* raw, std::string* path,
                                    std::string* reason) {
    auto reject = [&](const char* code) { path->clear(); *reason = code; return false; };
    if (!raw || !*raw) return reject("TexturePathEmpty");
    std::string value(raw);
    if (value.compare(0, 8, "Content/") != 0 ||
        value.find('\\') != std::string::npos ||
        value.find("..") != std::string::npos ||
        value.find("//") != std::string::npos)
        return reject("TexturePathUnsafe");
    for (unsigned char c : value)
        if (c < 32 || c == 127) return reject("TexturePathControlCharacter");
    const auto slash = value.rfind('/');
    const auto file = value.substr(slash + 1);
    if (file.empty()) return reject("TextureFileNameEmpty");
    if (file.find('.') == std::string::npos) value += ".png";
    else if (file.size() <= 4 || file.substr(file.size() - 4) != ".png")
        return reject("TextureExtensionUnsupported");
    if (value.size() > 160) return reject("TexturePathTooLong");
    *path = value;
    reason->clear();
    return true;
}
}
