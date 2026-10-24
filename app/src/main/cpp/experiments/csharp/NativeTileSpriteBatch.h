#pragma once
#include "experiments/csharp/NativeTileSpriteWire.h"
#include "il2cpp/Types.h"
#include <string>

namespace bl::csharp::tile_sprite {

bool bindNativeSpriteBatch(std::string* error);
bool nativeSpriteBatchIsOpen(Il2CppObject* spriteBatch);
bool drawNativeTileSprite(Il2CppObject* spriteBatch, const Sprite& command,
                          float screenX, float screenY);
void resetNativeSpriteBatch();

}
