#pragma once
#include "experiments/csharp/NativeTileSpriteWire.h"
#include "experiments/csharp/NativeTileParticleWire.h"
#include <cstdint>
#include <string>

namespace bl::csharp::tile_sprite {
bool installNativeTileSpritePass(std::string* error);
bool stageNativeTileSprites(uint64_t frame,const Batch& batch);
bool stageNativeTileSpritesForCurrentFrame(const Batch& batch);
bool bindNativeTileDust(std::string* error);
bool stageNativeTileParticlesForCurrentFrame(const tile_particle::Batch& batch);
void resetNativeTileSpritePass();
}
