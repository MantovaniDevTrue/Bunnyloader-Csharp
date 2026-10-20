#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::gore {

struct Source {
    std::string name;
    std::string relativePath;
    std::string absolutePath;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t byteCount = 0;
};

// Descobre somente Content/**/Gores/*.png. Nenhuma chamada IL2CPP, MonoVM
// ou QuickJS acontece aqui: a etapa de descoberta pode ser testada no host.
// Em caso de erro, "out" permanece inalterado.
bool discover(const std::string& modDirectory, std::vector<Source>* out,
              std::string* error);

}
