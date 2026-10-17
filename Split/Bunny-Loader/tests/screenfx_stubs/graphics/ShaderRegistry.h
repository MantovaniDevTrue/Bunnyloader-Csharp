#pragma once
#include <string>
namespace bl::graphics {
class ShaderRegistry {
public:
    static int Register(const std::string&, const std::string&);
};
}
