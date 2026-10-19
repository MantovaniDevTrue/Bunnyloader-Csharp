#pragma once
#include "experiments/csharp/NativeTileSpriteWire.h"
#include <cstdint>
#include <string>

namespace bl::csharp::tile_sprite {
bool installNativeTileSpritePass(std::string* error);
bool stageNativeTileSprites(uint64_t frame,const Batch& batch);
bool stageNativeTileSpritesForCurrentFrame(const Batch& batch);
void resetNativeTileSpritePass();
}
