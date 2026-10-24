// Production flag publisher, with controlled IL2CPP metadata and real compiled
// ExampleMod wire. No QuickJS is linked or initialized by this executable.
#include "experiments/csharp/NativeTileFlags.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>
struct Il2CppClass {const char* ns;const char* name;int size;bool value;int rank=0;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;uint32_t flags=0x10;Il2CppArray* data=nullptr;};
bl::il2cpp::Api fixture;
Il2CppClass boolean{"System","Boolean",1,true},integer{"System","Int32",4,true},
    boolArray{"System","Boolean[]",8,false,1,&boolean},
    mainClass{"Terraria","Main",32,false},setsClass{"Terraria.ID","Sets",32,false},
    wiringClass{"Terraria.ID","Wiring",32,false};
std::map<Il2CppClass*,std::map<std::string,FieldInfo>> fields;
std::vector<std::unique_ptr<char[]>> arrays;
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& t){return t.name=="Main"?&mainClass:t.name=="TileID"?&setsClass:nullptr;}
Il2CppClass* findNested(Il2CppClass* outer,std::string_view name){return outer==&setsClass&&name=="Wiring"?&wiringClass:nullptr;}
FieldInfo* findField(Il2CppClass* c,std::string_view n){auto& m=fields[c];auto i=m.find(std::string(n));return i==m.end()?nullptr:&i->second;}
}
Il2CppArray* allocate(size_t length=2200) {
 auto memory=std::make_unique<char[]>(sizeof(Il2CppArray)+length+16);
 std::memset(memory.get(),0,sizeof(Il2CppArray)+length+16);
 auto* a=reinterpret_cast<Il2CppArray*>(memory.get());a->obj.klass=&boolArray;a->length=length;
 arrays.push_back(std::move(memory));return a;
}
void clear(){for(auto& [c,m]:fields)for(auto& [n,f]:m)std::memset(arrayData(f.data),0,2200);}
bool allZero(){for(auto& [c,m]:fields)for(auto& [n,f]:m)for(size_t i=0;f.data&&i<2200;++i)if(static_cast<uint8_t*>(arrayData(f.data))[i])return false;return true;}
int main(int argc,char** argv) {
 using namespace bl::csharp::tile_flags;
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};
 fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=1;return c->size;};
 fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};
 fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
 fixture.field_static_get_value=[](FieldInfo* f,void* p){std::memcpy(p,&f->data,sizeof(f->data));};
 for(const char* n:{"tileTable","tileSolidTop","tileNoAttach","tileLavaDeath","tileObsidianKill","tileFrameImportant","tileSolid","tileNoSunLight","tileBlockLight","tileMergeDirt","tileLighted","tileWaterDeath","tileCut"})fields[&mainClass][n]={{&boolArray},0x10,allocate()};
 for(const char* n:{"DisableSmartCursor","HasOutlines","IgnoredByNpcStepUp","ChecksForMerge","Campfires","InteractableByNPCs","CommonSapling","SwaysInWindBasic"})fields[&setsClass][n]={{&boolArray},0x10,allocate()};
 for(const char* n:{"IsAMechanism","IgnoreWhenValidatingTraps"})fields[&wiringClass][n]={{&boolArray},0x10,allocate()};
 std::string error;
 assert(apply(2195,supportedMask,&error));
 for(auto& [c,m]:fields)for(auto& [n,f]:m){auto* bytes=static_cast<uint8_t*>(arrayData(f.data));assert(bytes[2195]==1&&bytes[2194]==0&&bytes[2196]==0);}
 clear();
 assert(apply(2195,512,&error));
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileSolid"].data))[2195]==1);
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileNoSunLight"].data))[2195]==1);
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileBlockLight"].data))[2195]==0);
 clear();
 assert(apply(2195,1024,&error));
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileSolid"].data))[2195]==0);
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileNoSunLight"].data))[2195]==0);
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileBlockLight"].data))[2195]==1);
 clear();
 auto sunlight=fields[&mainClass].extract("tileNoSunLight");
 assert(!apply(2195,512|1024,&error)&&error.find("tileNoSunLight")!=std::string::npos&&allZero());
 fields[&mainClass].insert(std::move(sunlight));
 auto& sunlightField=fields[&mainClass]["tileNoSunLight"];
 sunlightField.flags=0;
 assert(!apply(2195,512|1024,&error)&&error.find("StorageMismatch")!=std::string::npos&&allZero());
 sunlightField.flags=0x10;
 clear();
 assert(apply(2195,256,&error)&&allZero());
 clear();
 assert(apply(2195,65536,&error));
 for(const char* n:{"Campfires","InteractableByNPCs"})
  assert(static_cast<uint8_t*>(arrayData(fields[&setsClass][n].data))[2195]==1);
 for(const char* n:{"IsAMechanism","IgnoreWhenValidatingTraps"})
  assert(static_cast<uint8_t*>(arrayData(fields[&wiringClass][n].data))[2195]==1);
 assert(static_cast<uint8_t*>(arrayData(fields[&mainClass]["tileSolid"].data))[2195]==0);
 clear();
 assert(apply(2195,131072,&error)&&allZero());
 assert(apply(2195,131072|262144|524288,&error));
 for(const char* n:{"CommonSapling","SwaysInWindBasic"})
  assert(static_cast<uint8_t*>(arrayData(fields[&setsClass][n].data))[2195]==1);
 clear();
 auto saplingFlag=fields[&setsClass].extract("CommonSapling");
 assert(!apply(2195,131072|262144,&error)&&error.find("CommonSapling")!=std::string::npos&&allZero());
 fields[&setsClass].insert(std::move(saplingFlag));
 clear();
 auto mechanism=fields[&wiringClass].extract("IsAMechanism");
 assert(!apply(2195,65536,&error)&&error.find("IsAMechanism")!=std::string::npos&&allZero());
 fields[&wiringClass].insert(std::move(mechanism));
 auto& trapField=fields[&wiringClass]["IgnoreWhenValidatingTraps"];
 trapField.flags=0;
 assert(!apply(2195,65536,&error)&&error.find("StorageMismatch")!=std::string::npos&&allZero());
 trapField.flags=0x10;
 assert(!apply(2195,1048576,&error)&&allZero());
 assert(!apply(-1,1,&error)&&allZero());assert(!apply(2195,1048576,&error)&&allZero());
 auto& last=fields[&setsClass]["ChecksForMerge"];
 last.flags=0;assert(!apply(2195,512|16384,&error)&&error.find("StorageMismatch")!=std::string::npos&&allZero());last.flags=0x10;
 last.type.cls=&integer;assert(!apply(2195,512|16384,&error)&&error.find("System.Int32")!=std::string::npos&&allZero());last.type.cls=&boolArray;
 last.data->length=2195;assert(!apply(2195,512|16384,&error)&&error.find("Index=2195")!=std::string::npos&&allZero());last.data->length=2200;
 auto* old=last.data;last.data=nullptr;assert(!apply(2195,512|16384,&error)&&error.find("ActualLength=Null")!=std::string::npos&&allZero());last.data=old;
 boolArray.rank=2;assert(!apply(2195,512,&error)&&allZero());boolArray.rank=1;
 boolean.size=4;assert(!apply(2195,512,&error)&&allZero());boolean.size=1;
 auto node=fields[&setsClass].extract("ChecksForMerge");assert(!apply(2195,512|16384,&error)&&error.find("Missing")!=std::string::npos&&allZero());fields[&setsClass].insert(std::move(node));
 assert(argc==2);std::ifstream wire(argv[1]);std::string row;bool found=false;
 while(std::getline(wire,row))if(row.starts_with("TILE\tExampleTile\t")) {
  std::istringstream input(row);std::string part;std::getline(input,part,'\t');std::getline(input,part,'\t');std::getline(input,part,'\t');
  assert(std::stoi(part)==(512|1024|2048|16384));assert(apply(2195,std::stoi(part),&error));found=true;
 }
 assert(found);
 // Exercise the real housing-list publisher as well. RoomNeeds uses Int32[];
 // a table indexed by the tile type would be the wrong storage contract.
 Il2CppClass roomClass{"Terraria.ID","RoomNeeds",32,false};
 static Il2CppClass intArray{"System","Int32[]",8,false,1,&integer};
 auto allocateInts=[](uintptr_t length){auto* a=allocate(length*4);a->length=length;a->obj.klass=&intArray;return a;};
 auto* initial=allocateInts(2);auto* initialValues=static_cast<int32_t*>(arrayData(initial));initialValues[0]=14;initialValues[1]=18;
 auto& housing=fields[&roomClass]["CountsAsTableTypes"];housing={{&intArray},0x10,initial};
 fixture.field_static_set_value=[](FieldInfo* f,void* p){f->data=static_cast<Il2CppArray*>(p);};
 static int resizeMode=0;
 Resize resize=[](Il2CppArray* source,uintptr_t length)->Il2CppArray* {
  if(resizeMode==1)return nullptr;
  if(resizeMode==2)return source;
  auto* result=allocate(length*4);result->length=length;result->obj.klass=&intArray;
  std::memcpy(arrayData(result),arrayData(source),source->length*4);
  if(resizeMode==3)static_cast<int32_t*>(arrayData(result))[0]=999;
  return result;
 };
 assert(appendHousing(2195,&roomClass,resize,&error));
 assert(housing.data!=initial&&housing.data->length==3&&initial->length==2&&initialValues[1]==18);
 auto* published=housing.data;assert(static_cast<int32_t*>(arrayData(published))[2]==2195);
 assert(appendHousing(2195,&roomClass,resize,&error)&&housing.data==published);
 for(resizeMode=1;resizeMode<=3;++resizeMode)assert(!appendHousing(2196,&roomClass,resize,&error)&&housing.data==published);
 resizeMode=0;housing.type.cls=&boolArray;
 assert(!appendHousing(2196,&roomClass,resize,&error)&&error.find("System.Boolean[]")!=std::string::npos&&housing.data==published);housing.type.cls=&intArray;
 housing.flags=0;assert(!appendHousing(2196,&roomClass,resize,&error)&&housing.data==published);housing.flags=0x10;
 integer.size=2;assert(!appendHousing(2196,&roomClass,resize,&error)&&housing.data==published);integer.size=4;
 housing.data=allocateInts(4096);auto* full=housing.data;
 assert(!appendHousing(2196,&roomClass,resize,&error)&&error.find("CapacityExceeded")!=std::string::npos&&housing.data==full);
 std::cout<<"TILE_FLAGS_PRODUCTION_PUBLISHER_CAMPFIRE_WIRING_ATOMICITY_HOUSING_NO_QUICKJS=PASS\n";
}
