#include "experiments/csharp/NativeTileParticleStylePolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace bl::csharp::tile_particle;
    Particle d{};
    d.type=754;d.x=3;d.y=4;d.dustType=31;d.width=4;d.height=8;
    d.velocityXScaleMilli=200;
    d.velocityYScaleMilli=200;
    d.velocityYAddMilli=-1000;
    d.alphaAdd=63;
    d.fadeInMilli=500;
    d.noGravity=1;
    assert(valid(d));
    float vx=0,vy=0;
    assert(applyMotion(d,5.f,3.f,&vx,&vy));
    assert(std::abs(vx-1.f)<0.001f&&std::abs(vy+0.4f)<0.001f);
    assert(!applyMotion(d,std::numeric_limits<float>::infinity(),0.f,&vx,&vy));
    assert(saturatingAlpha(220,63)==255);
    assert(saturatingAlpha(100,63)==163);
    assert(!safeFieldOffset(0,4,128));
    assert(!safeFieldOffset(124,8,128));
    assert(safeFieldOffset(sizeof(void*)*2,8,128));
    d.fadeInMilli=3001;
    assert(!valid(d));
    std::cout<<"CSHARP_TILE_PARTICLE_STYLE_POLICY=PASS\n";
}
