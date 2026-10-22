#include "content/backgrounds/SurfaceTables.h"

namespace bl::runtime::surface {
bool growTables(const std::array<TableTarget,4>& targets,size_t total,std::string* error,bool(*isCurrent)(void*),void* context) {
    return il2cpp::growPrimitiveArrays(targets,total,error,isCurrent,context,"SurfaceBackground");
}
}
