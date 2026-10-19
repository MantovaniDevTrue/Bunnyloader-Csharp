// Runs the production wall registrar/hooks against controlled Android metadata.
// No QuickJS implementation or VM is linked. The engine side is the test fixture.
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "experiments/csharp/NativeWallRuntime.h"
#include "content/tiles/TileAccess.h"
#include "content/walls/ModWalls.h"
#include "content/tiles/ModTileMap.h"
#include "il2cpp/GuardedCall.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
struct Il2CppClass {const char* ns;const char* name;int size;int alignment=4;bool value=false;int rank=0;Il2CppClass* element=nullptr;std::vector<FieldInfo*> fields;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {std::string name;Il2CppType type;uint32_t flags=0x10;size_t offset=0;uintptr_t storage=0;};
struct MethodInfo {void* pointer=nullptr;bool instance=false;std::vector<Il2CppType> params;};
static bl::il2cpp::Api fixture;
static std::map<std::string,Il2CppClass> classes;
static std::map<std::string,FieldInfo> fields;
static std::map<std::string,MethodInfo> methods;
static std::map<std::string,void*> hooks;
static std::vector<std::unique_ptr<char[]>> memory;
static std::vector<std::string> logs;
static int wallType=367,mapCommits=0,dustCreated=0,dustNative=0,lightCalls=0,frameOriginalCalls=0,animationOriginalCalls=0;
static int lastDust=-1;static bool mapReject=false,assetReady=true;
static uint16_t worldWalls[64]{};
static int numbers[64]{},frameXs[64]{},frameYs[64]{};
static bl::runtime::TileArrays world{};
static bl::runtime::WallsInstalledHook listener=nullptr;
static bool requested=false;
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& t){auto it=classes.find(std::string(t.name));return it==classes.end()?nullptr:&it->second;}
FieldInfo* findField(Il2CppClass* c,std::string_view n){if(!c)return nullptr;auto it=fields.find(std::string(c->name)+"."+std::string(n));return it==fields.end()?nullptr:&it->second;}
Signature parseSignature(std::string_view s){Signature result;result.name=std::string(s);return result;}
const MethodInfo* findMethodBySignature(Il2CppClass* c,const Signature& s,bool*,const MethodInfo**){if(!c)return nullptr;auto it=methods.find(std::string(c->name)+"."+s.name);return it==methods.end()?nullptr:&it->second;}
bool guardedCall(void(*body)(void*),void* arg,Caught*){body(arg);return true;}
std::string describeCaught(const Caught&){return "fixture exception";}
std::string describeException(Il2CppObject*){return "fixture managed exception";}
}
namespace bl::hook {
bool install(const MethodInfo* m,void* replacement,void** original){*original=m->pointer;for(const auto& [key,value]:methods)if(&value==m){hooks[key]=replacement;return true;}return false;}
}
namespace bl::log {void write(int,const char* fmt,...){char buffer[4096];va_list args;va_start(args,fmt);vsnprintf(buffer,sizeof(buffer),fmt,args);va_end(args);logs.emplace_back(buffer);}}
namespace bl::runtime {
bool modWallAssetReady(int){return assetReady;}
bool modWallsSettled(){return false;}
int wallTypeCount(){return 370;}
int modWallTypeByName(const std::string& mod,const std::string& name){return mod=="fixture"&&name=="IndependentWall"?wallType:-1;}
bool tileArrays(TileArrays* out){*out=world;return true;}
int wallTypeAt(int x,int y){return x>=0&&y>=0&&x<8&&y<8?worldWalls[y*8+x]:0;}
int wallTypeAtOffset(int32_t cell){return cell>=0&&cell<64?worldWalls[cell]:0;}
void setNativeWallsInstalledHook(WallsInstalledHook hook){listener=hook;}
void requestNativeWallDefaults(){requested=true;}
bool setModWallMapEntries(int,const std::vector<WallMapEntry>&){if(mapReject)return false;++mapCommits;return true;}
}
namespace bl::csharp {int basicItemForWall(const std::string& key){return key=="fixture/IndependentWall"?6224:-1;}}
#include "../../app/src/main/cpp/experiments/csharp/NativeWallRuntime.cpp"

static Il2CppArray* allocate(Il2CppClass* cls,size_t length,int bytes){auto data=std::make_unique<char[]>(sizeof(Il2CppArray)+length*bytes+16);std::memset(data.get(),0,sizeof(Il2CppArray)+length*bytes+16);auto* a=reinterpret_cast<Il2CppArray*>(data.get());a->obj.klass=cls;a->length=length;memory.push_back(std::move(data));return a;}
static FieldInfo& field(const std::string& owner,const char* name,const char* type,uintptr_t value=0,size_t offset=0,uint32_t flags=0x10){auto key=owner+"."+name;auto& f=fields[key];f={name,{&classes.at(type)},flags,offset,value};classes.at(owner).fields.push_back(&f);return f;}
static void engineArea(int32_t,int32_t,int32_t,int32_t,const MethodInfo*){}
static void engineAnimate(const MethodInfo*){++animationOriginalCalls;}
static void engineFrame(int32_t,int32_t,bool,const MethodInfo*){++frameOriginalCalls;}
static void engineDust(int32_t,int32_t,uint32_t,const MethodInfo*){++dustNative;}
static int32_t engineDrop(uint32_t,const MethodInfo*){return 77;}
static void engineKill(int32_t x,int32_t y,bool,const MethodInfo*){for(int n=0;n<20;++n)bl::csharp::walls::dust(x,y,static_cast<uint32_t>(y*8+x),nullptr);}
static void engineLight(int32_t,int32_t,float r,float g,float b,const MethodInfo*){assert(r==.1f&&g==.5f&&b==0);++lightCalls;}
static uint8_t getNumber(int32_t* cell,const MethodInfo*){return static_cast<uint8_t>(numbers[*cell]);}
static int32_t getX(int32_t* cell,const MethodInfo*){return frameXs[*cell];}
static int32_t getY(int32_t* cell,const MethodInfo*){return frameYs[*cell];}
static void putNumber(int32_t* cell,uint8_t v,const MethodInfo*){numbers[*cell]=v;}
static void putX(int32_t* cell,int32_t v,const MethodInfo*){frameXs[*cell]=v;}
static void putY(int32_t* cell,int32_t v,const MethodInfo*){frameYs[*cell]=v;}
static void method(const char* owner,const char* signature,void* pointer,bool instance=false,std::vector<Il2CppType> params={}){methods[std::string(owner)+"."+signature]={pointer,instance,std::move(params)};}
static void setup(){
 auto cls=[](const char* name,int size,bool value=false,int alignment=4,const char* ns="Terraria"){classes[name]={ns,name,size,alignment,value,0,nullptr,{}};};
 for(const char* name:{"Main","WorldGen","Framing","Lighting","Dust","WallID","DustID"})cls(name,32);
 cls("Boolean",1,true,1,"System");cls("Byte",1,true,1,"System");cls("Int32",4,true,4,"System");cls("Single",4,true,4,"System");
 cls("Tile",4,true);cls("Point",8,true,4,"Microsoft.Xna.Framework");cls("Vector2",8,true,4,"Microsoft.Xna.Framework");cls("Color",4,true,1,"Microsoft.Xna.Framework.Graphics");
 for(const char* name:{"Boolean","Byte","Int32","Point"}){std::string array=std::string(name)+"[]";classes[array]={classes[name].ns,nullptr,8,8,false,1,&classes[name],{}};classes[array].name=classes.find(array)->first.c_str();}
 classes["Point[][]"]={"Microsoft.Xna.Framework","Point[][]",8,8,false,1,&classes["Point[]"],{}};
 field("Point","X","Int32",0,0,0);field("Point","Y","Int32",0,4,0);field("Vector2","X","Single",0,0,0);field("Vector2","Y","Single",0,4,0);
 for(const char* name:{"wallHouse","wallBlend","wallFrame","wallFrameCounter"}){const char* type=std::strcmp(name,"wallHouse")==0?"Boolean[]":std::strcmp(name,"wallBlend")==0?"Int32[]":"Byte[]";field("Main",name,type,reinterpret_cast<uintptr_t>(allocate(&classes[type],370,std::strcmp(type,"Int32[]")==0?4:1)));}
 field("Main","dayTime","Boolean",1);field("WallID","CogWall","Int32",155);field("DustID","Stone","Int32",1);field("DustID","GemEmerald","Int32",61);
 auto* rows=allocate(&classes["Point[][]"],1,sizeof(void*));auto* row=allocate(&classes["Point[]"],3,8);auto* coordinates=static_cast<int32_t*>(arrayData(row));for(int n=0;n<6;++n)coordinates[n]=n*18;static_cast<Il2CppArray**>(arrayData(rows))[0]=row;field("Framing","wallFrameLookup","Point[][]",reinterpret_cast<uintptr_t>(rows));
 method("Tile","byte wallFrameNumber()",reinterpret_cast<void*>(getNumber),true);
 method("Tile","int wallFrameX()",reinterpret_cast<void*>(getX),true);method("Tile","int wallFrameY()",reinterpret_cast<void*>(getY),true);
 method("Tile","void wallFrameNumber(byte wallFrameNumber)",reinterpret_cast<void*>(putNumber),true);
 method("Tile","void wallFrameX(int wallFrameX)",reinterpret_cast<void*>(putX),true);method("Tile","void wallFrameY(int wallFrameY)",reinterpret_cast<void*>(putY),true);
 method("Dust","int NewDust(Vector2 Position, int Width, int Height, int Type, float SpeedX, float SpeedY, int Alpha, Color newColor, float Scale)",reinterpret_cast<void*>(engineAnimate),false,{{&classes["Vector2"]},{},{},{},{},{},{},{&classes["Color"]}});
 method("Lighting","void AddLight(int i, int j, float r, float g, float b)",reinterpret_cast<void*>(engineLight));
 method("WorldGen","void KillWall(int i, int j, bool fail)",reinterpret_cast<void*>(engineKill));
 method("WorldGen","void KillWall_MakeWallDust(int i, int j, Tile tileCache)",reinterpret_cast<void*>(engineDust),false,{{},{},{&classes["Tile"]}});
 method("WorldGen","int KillWall_GetItemDrops(Tile tileCache)",reinterpret_cast<void*>(engineDrop),false,{{&classes["Tile"]}});
 method("Main","void DoUpdate_AnimateWalls()",reinterpret_cast<void*>(engineAnimate));method("Framing","void WallFrame(int i, int j, bool resetFrame)",reinterpret_cast<void*>(engineFrame));method("Lighting","void LightTiles(int firstX, int lastX, int firstY, int lastY)",reinterpret_cast<void*>(engineArea));
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=c->alignment;return c->size;};
 fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};fixture.field_static_get_value=[](FieldInfo* f,void* out){std::memcpy(out,&f->storage,f->type.cls->value?f->type.cls->size:sizeof(void*));};
 fixture.class_get_fields=[](Il2CppClass* c,void** iterator)->FieldInfo*{auto n=reinterpret_cast<uintptr_t>(*iterator);if(n>=c->fields.size())return nullptr;*iterator=reinterpret_cast<void*>(n+1);return c->fields[n];};
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};fixture.method_get_param=[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return i<m->params.size()?&m->params[i]:nullptr;};
 fixture.runtime_invoke=[](const MethodInfo*,void*,void** args,Il2CppObject** exception)->Il2CppObject*{*exception=nullptr;lastDust=*static_cast<int*>(args[3]);++dustCreated;return nullptr;};
 world.width=8;world.height=8;world.wall=worldWalls;worldWalls[9]=367;
}
int main(int argc,char** argv){
 setup();using namespace bl::csharp::walls;std::string error;Definition d;
 assert(argc==2);std::ifstream wire(argv[1]);std::string line;bool parsed=false;
 while(std::getline(wire,line))if(line.starts_with("WALL\t")){assert(parse(bl::csharp::basicSplit(line,'\t'),&d,&error));parsed=true;break;}
 assert(parsed);d.mod="fixture";d.name="IndependentWall";d.source="content/Wall.cs";d.type=367;
 auto invalid=d;invalid.mod="wrong";assert(!publish({invalid},&error)&&hooks.empty());
 classes["Color"].size=8;assert(!publish({d},&error)&&error.find("NativeValueAbiMismatch")!=std::string::npos&&hooks.empty());classes["Color"].size=4;
 fields["Main.wallHouse"].flags=0;assert(!publish({d},&error)&&error.find("wallHouse")!=std::string::npos&&hooks.empty());fields["Main.wallHouse"].flags=0x10;
 assert(publish({d},&error)&&requested&&listener&&hooks.size()==6);assert(loadStatus().pending==1);
 // Defaults commit after native table growth, never write past an old table.
 auto* housing=reinterpret_cast<Il2CppArray*>(fields["Main.wallHouse"].storage);housing->length=367;listener(367,369);assert(loadStatus().failed==1&&loadStatus().pending==0&&mapCommits==0);housing->length=370;
 assert(publish({d},&error));mapReject=true;listener(367,369);assert(loadStatus().failed==1&&static_cast<uint8_t*>(arrayData(housing))[367]==0);mapReject=false;
 assetReady=false;assert(publish({d},&error));listener(367,369);assert(loadStatus().failed==1&&mapCommits==0);assetReady=true;
 assert(publish({d},&error));listener(367,369);assert(loadStatus().failed==0&&loadStatus().pending==0&&mapCommits==1);assert(static_cast<uint8_t*>(arrayData(housing))[367]==1);
 assert(static_cast<int32_t*>(arrayData(reinterpret_cast<Il2CppArray*>(fields["Main.wallBlend"].storage)))[367]==155);
 static_cast<uint8_t*>(arrayData(housing))[367]=0;listener(367,369);assert(static_cast<uint8_t*>(arrayData(housing))[367]==1&&mapCommits==2);
 kill(1,1,false,nullptr);assert(dustCreated==10&&lastDust==61);kill(1,1,true,nullptr);assert(dustCreated==13);
 numbers[9]=1;dust(1,1,9,nullptr);assert(lastDust==1);assert(drop(9,nullptr)==6224);assert(drop(0,nullptr)==77);dust(0,0,0,nullptr);assert(dustNative==1);
 for(int n=0;n<5;++n){animate(nullptr);}
 auto* frames=reinterpret_cast<Il2CppArray*>(fields["Main.wallFrame"].storage);assert(static_cast<uint8_t*>(arrayData(frames))[367]==1&&animationOriginalCalls==5);
 for(int n=0;n<5;++n){animate(nullptr);}
 assert(static_cast<uint8_t*>(arrayData(frames))[367]==0);
 bool randomized=false;for(int n=0;n<100;++n){numbers[9]=0;frameXs[9]=0;frameYs[9]=18;frame(1,1,true,nullptr);if(numbers[9]){randomized=true;assert(frameXs[9]==numbers[9]*36&&frameYs[9]==numbers[9]*36+18);}}
 assert(randomized&&frameOriginalCalls==100);numbers[9]=0;frame(1,1,false,nullptr);assert(numbers[9]==0);
 area(0,7,0,7,nullptr);assert(lightCalls==0);fields["Main.dayTime"].storage=0;area(0,7,0,7,nullptr);assert(lightCalls==1);
 auto f=bl::csharp::basicSplit(line,'\t');f[10]="nan";assert(!parse(f,&invalid,&error)&&error.find("FrameChance")!=std::string::npos);
 assert(!validUtf8Text(std::string("\xc0\x80",2))&&!validUtf8Text(std::string("\xed\xa0\x80",3))&&!validUtf8Text(std::string("\xf4\x90\x80\x80",4)));
 assert(validUtf8Text("Example iluminação"));
 std::cout<<"WALL_PRODUCTION_REGISTER_TABLE_WIDTH_BOUNDS_FAILURE_ISOLATION_DUST_DROP_FRAMES_ANIMATION_LIGHT_NO_QUICKJS=PASS\n";
}
