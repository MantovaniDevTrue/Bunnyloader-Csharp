#pragma once
#include "il2cpp/Types.h"
#include <string>

namespace bl::csharp::tile_sprite {
bool bindNativeTileBatchLifecycle(std::string* error);
bool beginNativeTileBatch(Il2CppObject* batch,bool* started);
bool endNativeTileBatch(Il2CppObject* batch,bool started);
void resetNativeTileBatchLifecycle();
}
