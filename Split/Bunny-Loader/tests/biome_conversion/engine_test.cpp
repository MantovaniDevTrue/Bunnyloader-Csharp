#include "experiments/csharp/BiomeConversionEngine.h"
#include <cassert>
#include <cstdio>
using namespace bl::csharp;
struct World final : BiomeConversionWorld {
    bool live=true; int writes=0; int cells[32][32]{}; int walls[32][32]{};
    bool ready()override{return live;}
    bool read(int x,int y,int* t,int* w)override{if(x<0||y<0||x>=32||y>=32)return false;*t=cells[y][x];*w=walls[y][x];return true;}
    bool validTarget(int t,bool)override{return t>=0&&t<1000;}
    void write(int x,int y,int t,int w)override{++writes;if(t>=0)cells[y][x]=t;if(w>=0)walls[y][x]=w;}
};
int main(){
    BiomeConversionEngine engine;World w;
    const std::string a="BUNNY_BIOME_CONVERSION_V1\nC|22\nT|22|0|1\nW|22|2|3\n";
    assert(engine.publish("owner-a",a));assert(!engine.request("owner-a",22,8,8,2,true,true));
    engine.world(true);assert(!engine.publish("owner-b",a));assert(!engine.publish("owner-a",a+"T|22|0|2\n"));
    assert(engine.size()==1);assert(!engine.request("owner-b",22,8,8,2,true,true));
    w.walls[8][8]=2;assert(engine.request("owner-a",22,8,8,0,true,true));assert(engine.pump(w)==1);
    assert(w.cells[8][8]==1&&w.walls[8][8]==3&&w.cells[9][8]==0);
    w.cells[5][5]=-1;w.walls[5][5]=0;
    assert(engine.request("owner-a",22,5,5,0,true,true));assert(engine.pump(w)==0); // inactive tile / absent wall
    assert(engine.request("owner-a",22,12,12,2,true,false));assert(engine.pump(w)==13); // clipped square radius two
    assert(w.cells[10][10]==0&&w.cells[11][11]==1);
    assert(engine.publish("owner-b","BUNNY_BIOME_CONVERSION_V1\nC|23\nT|23|0|5\n"));
    assert(engine.request("owner-a",22,20,20,0,true,true));assert(engine.publish("owner-a",""));assert(engine.pump(w)==0);
    assert(w.cells[20][20]==0&&engine.size()==1);
    for(int i=0;i<64;++i)assert(engine.request("owner-b",23,20,20,0,true,false));
    assert(!engine.request("owner-b",23,20,20,0,true,false));
    engine.world(false);engine.world(true);assert(engine.pump(w)==0&&w.cells[20][20]==0);
    assert(!engine.request("owner-b",23,20,20,9,true,true));
    assert(engine.request("owner-b",23,20,20,0,true,false));w.live=false;assert(engine.pump(w)==0);
    w.live=true;engine.world(true);assert(engine.pump(w)==0);
    assert(!engine.publish("owner-b","BUNNY_BIOME_CONVERSION_V1\nC|23\nW|23|0|1\n"));
    assert(!engine.publish("owner-b","BUNNY_BIOME_CONVERSION_V1\nC|23\nT|23|1|65536\n"));
    assert(!engine.publish("owner-b","BUNNY_BIOME_CONVERSION_V1\nC|23\nT|24|1|2\n"));
    assert(engine.request("owner-b",23,20,20,0,true,false));assert(engine.pump(w)==1&&w.cells[20][20]==5);
    assert(engine.publish("owner-c","BUNNY_BIOME_CONVERSION_V1\nC|24\nT|24|0|9999\nW|24|2|0\n"));
    w.walls[22][22]=2;assert(engine.request("owner-c",24,22,22,0,true,true));
    assert(engine.pump(w)==1&&w.cells[22][22]==0&&w.walls[22][22]==0); // invalid target skipped, valid wall removal
    assert(!engine.publish("owner-c","BUNNY_BIOME_CONVERSION_V1\nC|24|\n"));
    std::puts("BIOME_CONVERSION_NATIVE_OWNERSHIP_BRUSH_BOUNDS_QUEUE_WORLD_UNLOAD_PASS");
}
