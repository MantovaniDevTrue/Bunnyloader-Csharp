#include "experiments/csharp/BiomeConversionEngine.h"
#include <cstdio>
using namespace bl::csharp;
static BiomeConversionEngine engine;
struct World final : BiomeConversionWorld {
    int tile=1,wall=2;
    bool ready()override{return true;}
    bool read(int,int,int* t,int* w)override{*t=tile;*w=wall;return true;}
    bool validTarget(int t,bool)override{return t>=0&&t<1000;}
    void write(int,int,int t,int w)override{if(t>=0)tile=t;if(w>=0)wall=w;}
};
static World world;
static bool rejectNext=false;
extern "C" void test_reject_publish(){rejectNext=true;}
extern "C" int test_count(){return static_cast<int>(engine.size());}
extern "C" int bunny_biome_conversion_publish(const char* owner,const char* wire){if(rejectNext){rejectNext=false;return 0;}return owner&&wire&&engine.publish(owner,wire);}
extern "C" int bunny_biome_conversion_request(const char* owner,int type,int x,int y,int size,int tiles,int walls){return owner&&engine.request(owner,type,x,y,size,tiles,walls);}
extern "C" void test_world_begin(){engine.world(true);}
extern "C" int test_pump(){return static_cast<int>(engine.pump(world));}
extern "C" int test_tile(){return world.tile;}
extern "C" int test_wall(){return world.wall;}
