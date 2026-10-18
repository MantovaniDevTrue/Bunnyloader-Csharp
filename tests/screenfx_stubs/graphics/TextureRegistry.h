#pragma once
#include <string>
namespace bl::graphics {
class TextureRegistry {
public:
    static int Load(const std::string& path, bool linear, bool repeat, int* width, int* height);
};
}
