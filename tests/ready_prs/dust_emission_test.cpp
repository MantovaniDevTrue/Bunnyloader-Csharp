#include "experiments/csharp/NativeDustEmission.cpp"
#include <cassert>
#include <cstdlib>
#include <cstdio>

struct Il2CppType { const char* name; };
struct FieldInfo { Il2CppType type; size_t offset; uint32_t flags; };
struct MethodInfo { bool valid; };
namespace {
struct Player { Il2CppObject object{}; float x=120,y=240; uint8_t active=1; } player;
struct Players { Il2CppArray array{{},nullptr,1}; Il2CppObject* slot=&player.object; } players;
FieldInfo playersField{{"Terraria.Player[]"},0,0x10};
FieldInfo positionField{{"Microsoft.Xna.Framework.Vector2"},offsetof(Player,x),0};
FieldInfo activeField{{"System.Boolean"},offsetof(Player,active),0};
MethodInfo spawn{true};
int calls=0;
bool spawnStatic=true;
const char* parameterName=nullptr;
const char* colorTypeName="Microsoft.Xna.Framework.Color";
bl::il2cpp::Api api;
}
namespace bl::log { void write(int,const char*,...){} }
namespace bl::il2cpp {
Api& api(){return ::api;}
std::string describeException(Il2CppObject*) { return "FixtureException"; }
Il2CppClass* findClass(const TypeRef&) {return reinterpret_cast<Il2CppClass*>(1);}
FieldInfo* findField(Il2CppClass*,std::string_view name) {
    return name=="player"?&playersField:name=="position"?&positionField:&activeField;
}
}
namespace bl::runtime {
int modDustTypeByName(const std::string& mod,const std::string& name) {
    return mod=="example-uid"&&name=="ExampleSparkDust"?332:-1;
}
}
int main() {
    api.type_get_name=[](const Il2CppType* t){return strdup(t->name);};
    api.il2cpp_free=[](void* p){free(p);};
    api.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    api.field_get_flags=[](FieldInfo* f){return f->flags;};
    api.field_get_offset=[](FieldInfo* f){return f->offset;};
    api.class_instance_size=[](Il2CppClass*){return static_cast<uint32_t>(sizeof(Player));};
    api.class_from_il2cpp_type=[](const Il2CppType*){return reinterpret_cast<Il2CppClass*>(1);};
    api.class_value_size=[](Il2CppClass*,uint32_t*){return int32_t{8};};
    api.class_get_parent=[](Il2CppClass*)->Il2CppClass*{return nullptr;};
    api.class_get_methods=[](Il2CppClass*,void** it)->const MethodInfo*{if(*it)return nullptr;*it=&spawn;return &spawn;};
    api.method_get_name=[](const MethodInfo*){return "NewDust";};
    api.method_get_param_count=[](const MethodInfo*){return uint32_t{9};};
    api.method_get_flags=[](const MethodInfo*,uint32_t*){return spawnStatic?uint32_t{0x10}:uint32_t{0};};
    api.method_get_return_type=[](const MethodInfo*)->const Il2CppType*{static Il2CppType t{"System.Int32"};return &t;};
    api.method_get_param=[](const MethodInfo*,uint32_t i)->const Il2CppType*{
        static Il2CppType types[]={{"Microsoft.Xna.Framework.Vector2"},{"System.Int32"},{"System.Int32"},{"System.Int32"},{"System.Single"},{"System.Single"},{"System.Int32"},{"Microsoft.Xna.Framework.Color"},{"System.Single"}};
        types[7].name=colorTypeName;
        return &types[i];
    };
    api.method_get_param_name=[](const MethodInfo*,uint32_t i)->const char*{
        static const char* names[]={"Position","Width","Height","Type","SpeedX","SpeedY","Alpha","newColor","Scale"};
        return parameterName?parameterName:names[i];
    };
    api.field_static_get_value=[](FieldInfo*,void* out){*static_cast<Il2CppArray**>(out)=&players.array;};
    api.runtime_invoke=[](const MethodInfo*,void*,void** args,Il2CppObject** error)->Il2CppObject*{
        assert(*static_cast<float*>(args[0])==120);assert(*static_cast<int*>(args[3])==332);
        *error=nullptr;++calls;return nullptr;
    };
    using namespace bl::csharp;
    assert(bunny_dust_emission_session()==0);
    assert(!bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,8));
    setNativeDustEmissionWorld(true,1);
    assert(!bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,17));
    assert(!bunny_dust_emit(1,"bad/mod","ExampleSparkDust",0,1));
    assert(bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,8));
    pumpNativeDustEmission();assert(calls==8);
    for(int i=0;i<32;++i)assert(bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,1));
    assert(!bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,1));
    setNativeDustEmissionWorld(false,1);pumpNativeDustEmission();assert(calls==8);
    setNativeDustEmissionWorld(true,2);assert(!bunny_dust_emit(1,"example-uid","ExampleSparkDust",0,1));
    assert(bunny_dust_emit(2,"example-uid","MissingDust",0,1));pumpNativeDustEmission();assert(calls==8);
    player.active=0;assert(bunny_dust_emit(2,"example-uid","ExampleSparkDust",0,1));pumpNativeDustEmission();assert(calls==8);
    player.active=1;refs={};spawnStatic=false;
    assert(bunny_dust_emit(2,"example-uid","ExampleSparkDust",0,1));pumpNativeDustEmission();assert(calls==8);
    spawnStatic=true;refs={};parameterName="renamedByAndroid";
    assert(bunny_dust_emit(2,"example-uid","ExampleSparkDust",0,1));pumpNativeDustEmission();assert(calls==9);
    parameterName=nullptr;refs={};
    colorTypeName="Microsoft.Xna.Framework.Graphics.Color";
    assert(bunny_dust_emit(2,"example-uid","ExampleSparkDust",0,1));
    pumpNativeDustEmission();assert(calls==9);
    colorTypeName="Microsoft.Xna.Framework.Color";
    player.active=1;positionField.type.name="System.Int64";refs={};
    assert(bunny_dust_emit(2,"example-uid","ExampleSparkDust",0,1));pumpNativeDustEmission();assert(calls==9);
    puts("DUST_EMISSION_GAME_THREAD_CONTRACT=PASS bounded=32 count=16 stale=Rejected typeMismatch=Rejected noQuickJS=1");
}
