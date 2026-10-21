#include "il2cpp/Api.h"
#include <string>
#include <cstring>
struct Il2CppClass { const char* name;int size;bool value=true;const char* ns="System"; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name;Il2CppType type;uint32_t flags=0x10;double number=1; };
#include "content/systems/SystemTimeRates.cpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <functional>
namespace {
bl::il2cpp::Api fixture;
Il2CppClass single{"Single",4},doubleType{"Double",8},integer{"Int32",4},bad{"Int64",8},owner{"Main",0,false,"Terraria"};
FieldInfo day{"dayRate",{&single}},tile{"desiredWorldTilesUpdateRate",{&integer}};
int gameTid=0,writes=0,reads=0,vanillaCalls=0;
}
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name){return ns=="Terraria" && name=="Main"?&owner:nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls!=&owner?nullptr:name==day.name?&day:name==tile.name?&tile:nullptr;}
}
namespace bl::runtime {int gameThreadId(){return gameTid;}}
int main(){
 using namespace bl::runtime::systems;
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};
 fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->size);};
 fixture.field_static_get_value=[](FieldInfo* f,void* out){
  ++reads;const auto* cls=f->type.cls;
  if(cls==&single){float n=f->number;std::memcpy(out,&n,4);}
  else if(cls==&integer){int32_t n=f->number;std::memcpy(out,&n,4);}
  else {assert(cls==&doubleType);std::memcpy(out,&f->number,8);}
 };
 fixture.field_static_set_value=[](FieldInfo* f,void* in){
  ++writes;const auto* cls=f->type.cls;
  if(cls==&single){float n;std::memcpy(&n,in,4);f->number=n;}
  else if(cls==&integer){int32_t n;std::memcpy(&n,in,4);f->number=n;}
  else {assert(cls==&doubleType);std::memcpy(&f->number,in,8);}
 };
 std::string error;TimeRates r;
 assert(!beginTimeRates(&error) && error.find("WrongThread")!=error.npos);
 tile.type.cls=&bad;
 assert(!prepareTimeRates(&error) && error.find("Actual=System.Int64:8")!=error.npos && !writes && !reads);
 tile.type.cls=&integer;day.flags=0x30;
 assert(!prepareTimeRates(&error) && error.find("NativeStorageMismatch")!=error.npos && !writes && !reads);
 day.flags=0x10;assert(prepareTimeRates(&error));assert(!writes && !reads);
 gameTid=gettid();assert(!readTimeRates(&r,&error));
 assert(!publishTimeRates({},&error));assert(!endTimeRates(&error));assert(std::isnan(clockRate()));
 bool workerAccepted=true;std::thread worker([&]{std::string workerError;workerAccepted=beginTimeRates(&workerError);assert(workerError.find("WrongThread")!=workerError.npos);});worker.join();assert(!workerAccepted);
 // Compose two independent language wrappers in both registration orders.
 // The inner time is .25 but events .75; the outer must see both unrounded.
 for(bool csFirst:{false,true}){
  observeTimeRateMenu(true);observeTimeRateMenu(false);
  const auto inner=[&]{assert(beginTimeRates(&error));++vanillaCalls;day.number=tile.number=1;
   assert(readTimeRates(&r,&error));assert(r.time==1 && r.tiles==1 && r.events==1);
   assert(publishTimeRates(csFirst?TimeRates{.25,.5,.75}:TimeRates{.5,.25,.25},&error));
   const int before=writes;assert(endTimeRates(&error));assert(writes==before);
  };
  for(int frame=0;frame<4;++frame){
   assert(beginTimeRates(&error));inner();assert(readTimeRates(&r,&error));
   assert(r.time==(csFirst?.25:.5) && r.events==(csFirst?.75:.25));
   r.time+=csFirst?.5:.25;r.tiles+=csFirst?.25:.5;r.events+=csFirst?.25:.75;
   assert(publishTimeRates(r,&error));const int before=writes;assert(endTimeRates(&error));
   assert(writes==before+2 && clockRate()==.75 && day.number==1);
   assert(tile.number==(frame==0?0:1));
  }
 }
 assert(vanillaCalls==8);
 // Failure never commits a rejected triple or truncates native memory.
 observeTimeRateMenu(true);observeTimeRateMenu(false);
 assert(std::isnan(clockRate()));assert(beginTimeRates(&error));
 assert(!publishTimeRates({1,-1,1},&error));int before=writes;
 assert(endTimeRates(&error) && writes==before && std::isnan(clockRate()));
 // Single counters never round above Int32.MaxValue before native loops.
 assert(beginTimeRates(&error));assert(publishTimeRates({1,1,2147483647.},&error));assert(endTimeRates(&error));
 assert(day.number==2147483520.);
 // Double fields use all eight bytes, and fractional values survive inner read.
 dayField={&day,SystemStageFieldKind::Double};tileField={&tile,SystemStageFieldKind::Double};
 day.type.cls=tile.type.cls=&doubleType;day.number=2.5;tile.number=1.75;
 assert(beginTimeRates(&error));assert(readTimeRates(&r,&error));assert(r.time==2.5 && r.tiles==1.75 && r.events==2.5);
 assert(publishTimeRates({2147483647.,2147483647.,2147483647.},&error));assert(endTimeRates(&error));
 assert(day.number==2147483647. && tile.number==2147483647.);
 for(int i=0;i<16;++i) assert(beginTimeRates(&error));
 assert(!beginTimeRates(&error) && error.find("NestingLimit")!=error.npos);
 for(int i=0;i<16;++i) assert(endTimeRates(&error));
 std::cout<<"PRODUCTION_SHARED_TIME_RATES_TYPED_FIELDS_BOTH_LANGUAGES_FRACTIONS_THREAD_FAILURE_WORLD_BOUNDARY=PASS\n";
}
