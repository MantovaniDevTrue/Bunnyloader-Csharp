#include "experiments/csharp/NativeTileParticleReply.h"
#include "experiments/csharp/NativeTileParticleQueue.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp;
    tile_particle::Batch particles{};
    assert(tile_particle::parse("PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295",&particles));
    assert(particles.count==1&&particles.items[0].speedYMilli==-1500);
    assert(!particles.items[0].styled);
    const auto styled="PARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,1,500,200,200,-1000,4,63";
    assert(tile_particle::parse(styled,&particles));
    assert(particles.items[0].styled&&particles.items[0].noGravity==1);
    assert(particles.items[0].fadeInMilli==500&&particles.items[0].velocityYScaleMilli==200);
    assert(particles.items[0].velocityYAddMilli==-1000&&particles.items[0].alphaAdd==63);
    assert(tile_particle::owned(particles,754,3,4));
    assert(!tile_particle::owned(particles,755,3,4));
    assert(!tile_particle::owned(particles,754,3,5));
    for(const auto* invalid:{
        "PARTICLES|0",
        "PARTICLES|3",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295|",
        "PARTICLES|1|754,3,4,5000,2,-4,4,8,0,-1500,100,1000,4294967295",
        "PARTICLES|1|754,3,4,31,65,-4,4,8,0,-1500,100,1000,4294967295",
        "PARTICLES|1|754,3,4,31,2,-4,0,8,0,-1500,100,1000,4294967295",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,256,1000,4294967295",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,3001,4294967295",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967296",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,1,500,200,200,-1000,4",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,1,500,200,200,-1000,4,63,0",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,2,500,200,200,-1000,4,63",
        "PARTICLES|1|754,3,4,31,2,-4,4,8,0,0,100,1000,4294967295,1,500,200,200,-1000,4,101"
    })assert(!tile_particle::parse(invalid,&particles));
    tile_draw::Data draw{};
    tile_sprite::Batch sprites{};
    bool hasSprites=false,hasParticles=false;
    const auto base="DRAW|0|0|16|16|0|0|0|0";
    assert(tile_particle::parseReply(base,&draw,&sprites,&hasSprites,&particles,&hasParticles));
    assert(!hasSprites&&!hasParticles);
    assert(tile_particle::parseReply(
        "DRAW|0|0|16|16|0|0|0|0\nPARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295",
        &draw,&sprites,&hasSprites,&particles,&hasParticles));
    assert(!hasSprites&&hasParticles&&particles.count==1);
    assert(tile_particle::parseReply(
        "DRAW|0|0|16|16|0|0|0|0\nSPRITES|1|754,3,4,0,0,16,16,0,0,4294967295\nPARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295",
        &draw,&sprites,&hasSprites,&particles,&hasParticles));
    assert(hasSprites&&hasParticles);
    assert(!tile_particle::parseReply(
        "DRAW|0|0|16|16|0|0|0|0\nPARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295\nSPRITES|1|754,3,4,0,0,16,16,0,0,4294967295",
        &draw,&sprites,&hasSprites,&particles,&hasParticles));
    assert(tile_particle::parse("PARTICLES|1|754,3,4,31,2,-4,4,8,0,-1500,100,1000,4294967295",&particles));
    tile_particle::ParticleQueue queue;
    assert(!queue.stage(0,particles));
    assert(queue.stage(1,particles)&&queue.pendingFor(1));
    assert(!queue.pendingFor(2));
    std::array<tile_particle::Particle,tile_particle::kMaxFrameParticles> output{};
    size_t amount=0;
    assert(queue.take(1,&output,&amount)&&amount==1);
    for(size_t i=0;i<tile_particle::kMaxFrameParticles;++i)assert(queue.stage(2,particles));
    assert(!queue.stage(2,particles));
    assert(queue.take(2,&output,&amount)&&amount==tile_particle::kMaxFrameParticles);
    assert(queue.stage(3,particles)&&!queue.stage(2,particles));
    queue.clear();
    assert(!queue.pendingFor(3));
    std::cout<<"CSHARP_TILE_PARTICLES_WIRE_QUEUE=PASS\n";
}
