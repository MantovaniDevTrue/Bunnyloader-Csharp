#include "experiments/csharp/NativeBiomeConversion.h"
#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
namespace bl::log { void write(int,const char*,...) {} }

struct Il2CppType { const char* name; };
struct MethodInfo { void* pointer; const char* name; std::vector<Il2CppType*> args; bool isStatic; };
struct Il2CppClass { std::string name; std::vector<MethodInfo*> methods; };
struct FieldInfo { std::string name; Il2CppType* type; };
namespace {
Il2CppType u16{"System.UInt16"},i32{"System.Int32"},boolean{"System.Boolean"},array{"System.Boolean[]"},nothing{"System.Void"};
uint32_t lookup[256]{};uint16_t types[1024]{1},walls[256]{};int nextType=0,tileWrites=0,wallWrites=0,frames=0;
int mode=0;uint8_t menu=0;bool bad=false,badFraming=false;Il2CppArray limits{};
void setTile(int32_t* at,uint16_t type,const MethodInfo*){++tileWrites;lookup[*at]=++nextType;types[nextType]=type;}
void setWall(int32_t* at,uint16_t type,const MethodInfo*){++wallWrites;walls[*at]=type;}
void frame(int32_t,int32_t,bool,const MethodInfo*){++frames;}
MethodInfo mt{reinterpret_cast<void*>(setTile),"set_type",{&u16},false},mw{reinterpret_cast<void*>(setWall),"set_wall",{&u16},false},
 ft{reinterpret_cast<void*>(frame),"SquareTileFrame",{&i32,&i32,&boolean},true},fw{reinterpret_cast<void*>(frame),"WallFrame",{&i32,&i32,&boolean},true};
Il2CppClass tile{"Tile",{&mt,&mw}},world{"WorldGen",{&ft}},framing{"Framing",{&fw}},mainClass{"Main",{}};
FieldInfo fmenu{"gameMenu",&boolean},fnet{"netMode",&i32},ftiles{"tileSolid",&array},fwalls{"wallHouse",&array};
bl::il2cpp::Api api;
}
namespace bl::il2cpp {
Api& api(){return ::api;}
Il2CppClass* findClass(const TypeRef& t){if(t.name=="Tile")return &tile;if(t.name=="WorldGen")return &world;if(t.name=="Framing")return &framing;if(t.name=="Main")return &mainClass;return nullptr;}
FieldInfo* findField(Il2CppClass*,std::string_view n){if(n=="gameMenu")return &fmenu;if(n=="netMode")return &fnet;if(n=="tileSolid")return &ftiles;if(n=="wallHouse")return &fwalls;return nullptr;}
}
namespace bl::runtime {
bool tileArrays(TileArrays* a){a->width=16;a->height=16;a->lookup=lookup;a->type=types;a->wall=walls;return true;}
int tileTypeAt(int x,int y){return types[lookup[y*16+x]];}
int wallTypeAt(int x,int y){return walls[y*16+x];}
}
extern "C" int bunny_biome_conversion_publish(const char*,const char*);
extern "C" int bunny_biome_conversion_request(const char*,int,int,int,int,int,int);
int main(int argc,char** argv){
 bad=argc>1&&std::strcmp(argv[1],"bad-abi")==0;
 badFraming=argc>1&&std::strcmp(argv[1],"missing-framing")==0;
 if(badFraming)framing.methods.clear();
 limits.length=1000;
 api.class_get_methods=[](Il2CppClass* c,void** it)->const MethodInfo*{size_t n=reinterpret_cast<size_t>(*it);if(n>=c->methods.size())return nullptr;*it=reinterpret_cast<void*>(n+1);return c->methods[n];};
 api.method_get_name=[](const MethodInfo* m){return m->name;};
 api.method_get_param_count=[](const MethodInfo* m)->uint32_t{return m->args.size();};
 api.method_get_param=[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return m->args[i];};
 api.method_get_return_type=[](const MethodInfo*)->const Il2CppType*{return &nothing;};
 api.method_get_flags=[](const MethodInfo* m,uint32_t*)->uint32_t{return m->isStatic?0x10:0;};
 api.field_get_type=[](FieldInfo* f)->const Il2CppType*{return f->type;};
 api.field_get_flags=[](FieldInfo*)->uint32_t{return 0x10;};
 api.type_get_name=[](const Il2CppType* t){return strdup(t->name);};api.il2cpp_free=[](void* p){free(p);};
 api.class_value_size=[](Il2CppClass*,uint32_t*)->int32_t{return bad?8:4;};
 api.field_static_get_value=[](FieldInfo* f,void* out){if(f==&fmenu)std::memcpy(out,&menu,1);else if(f==&fnet)std::memcpy(out,&mode,4);else{Il2CppArray* p=&limits;std::memcpy(out,&p,sizeof(p));}};
 using namespace bl::csharp;
 assert(bunny_biome_conversion_publish("owner","BUNNY_BIOME_CONVERSION_V1\nC|22\nT|22|1|2\nW|22|3|4\n"));
 beginBiomeConversionWorld();walls[8*16+8]=3;
 assert(bunny_biome_conversion_request("owner",22,8,8,0,1,1));pumpBiomeConversionsOnGameThread();
 if(bad||badFraming){
  assert(tileWrites==0&&wallWrites==0&&frames==0&&!nativeBiomeConversionReady());
  puts(bad?"BIOME_CONVERSION_BAD_TILE_ABI_FAILS_CLOSED_PASS":"BIOME_CONVERSION_MISSING_FRAMING_FAILS_CLOSED_PASS");return 0;
 }
 assert(nativeBiomeConversionReady());
 assert(types[lookup[8*16+8]]==2&&types[lookup[9*16+8]]==1&&types[0]==1);
 assert(walls[8*16+8]==4&&tileWrites==1&&wallWrites==1&&frames==2);
 assert(bunny_biome_conversion_request("owner",22,9,9,0,1,1));mode=1;pumpBiomeConversionsOnGameThread();
 assert(tileWrites==1);mode=0;beginBiomeConversionWorld();
 assert(bunny_biome_conversion_request("owner",22,9,9,0,1,1));menu=1;pumpBiomeConversionsOnGameThread();assert(tileWrites==1);
 puts("BIOME_CONVERSION_PRODUCTION_ADAPTER_COPY_ON_WRITE_FRAME_SINGLEPLAYER_PASS");
}
