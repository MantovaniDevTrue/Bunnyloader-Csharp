#include "il2cpp/Api.h"
#include <vector>
#include <string>
struct FieldInfo;
struct Il2CppClass {
    const char* ns; const char* name; int size; bool value;
    std::vector<FieldInfo*> fields{};
};
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType type; size_t offset; uint32_t flags; };
struct MethodInfo { std::vector<Il2CppType> params; };
#include "experiments/csharp/NativeInvasionHud.cpp"
#include <cassert>
#include <iostream>

namespace {
bl::il2cpp::Api fixture;
Il2CppClass byteClass{"System","Byte",1,true}, intClass{"System","Int32",4,true};
Il2CppClass mainClass{"Terraria","Main",0,false}, utilsClass{"Terraria","Utils",0,false};
Il2CppClass texturesClass{"Terraria.GameContent","TextureAssets",0,false};
Il2CppClass batchClass{"Microsoft.Xna.Framework.Graphics","SpriteBatch",0,false};
Il2CppClass colorClass{"Microsoft.Xna.Framework.Graphics","Color",4,true};
Il2CppClass otherColorClass{"Microsoft.Xna.Framework","Color",4,true};
Il2CppClass rectangleClass{"Microsoft.Xna.Framework","Rectangle",16,true};
FieldInfo channels[]={{"R",{&byteClass},18,0},{"G",{&byteClass},17,0},
                      {"B",{&byteClass},16,0},{"A",{&byteClass},19,0}};
FieldInfo rectangleFields[]={{"X",{&intClass},16,0},{"Y",{&intClass},20,0},
                             {"Width",{&intClass},24,0},{"Height",{&intClass},28,0}};
FieldInfo mainFields[]={{"spriteBatch",{&batchClass},0,0x10},{"screenWidth",{&intClass},0,0x10},
                        {"screenHeight",{&intClass},0,0x10},{"gameMenu",{&byteClass},0,0x10}};
FieldInfo pixel{"MagicPixel",{&texturesClass},0,0x10};
MethodInfo rectDraw{{{nullptr},{&rectangleClass},{&colorClass}}};
MethodInfo textDraw{{{&batchClass},{nullptr},{nullptr},{&colorClass},{nullptr},{nullptr},{nullptr},{nullptr}}};
MethodInfo drawHook{}, getter{};
Il2CppObject batchObject{}, assetObject{}, textureObject{};
int hooks=0, rectangles=0, texts=0, missingClassLookups=0;
}
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& type) {
    for(auto* cls:{&mainClass,&utilsClass,&texturesClass,&batchClass,&rectangleClass})
        if(type.ns==cls->ns && type.name==cls->name)return cls;
    ++missingClassLookups;
    return nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    for(auto* field:cls->fields)if(name==field->name)return field;
    return nullptr;
}
Signature parseSignature(std::string_view text) {
    Signature s;s.name=text.find("DrawBorderString")!=text.npos?"DrawBorderString":
        text.find("InvasionProgressBars")!=text.npos?"HUD":"Draw";s.valid=true;return s;
}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& s,bool*,const MethodInfo**) {
    return s.name=="DrawBorderString"?&textDraw:s.name=="HUD"?&drawHook:&rectDraw;
}
}
namespace bl::hook {
bool install(const MethodInfo* m,void*,void** original){assert(m==&drawHook);*original=nullptr;++hooks;return true;}
}
int main() {
    colorClass.fields={&channels[0],&channels[1],&channels[2],&channels[3]};
    rectangleClass.fields={&rectangleFields[0],&rectangleFields[1],&rectangleFields[2],&rectangleFields[3]};
    mainClass.fields={&mainFields[0],&mainFields[1],&mainFields[2],&mainFields[3]};texturesClass.fields={&pixel};
    fixture.class_get_name=[](Il2CppClass* c){return c->name;};
    fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    fixture.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->size);};
    fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
    fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
    fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
    fixture.class_get_fields=[](Il2CppClass* c,void** it)->FieldInfo* {
        const auto i=reinterpret_cast<uintptr_t>(*it);
        if(i>=c->fields.size())return nullptr;
        *it=reinterpret_cast<void*>(i+1);return c->fields[i];
    };
    fixture.method_get_param=[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return &m->params.at(i);};
    fixture.field_static_get_value=[](FieldInfo* f,void* out){
        if(f==&mainFields[0])*static_cast<Il2CppObject**>(out)=&batchObject;
        else if(f==&pixel)*static_cast<Il2CppObject**>(out)=&assetObject;
        else if(f==&mainFields[3])*static_cast<bool*>(out)=false;
        else *static_cast<int*>(out)=f==&mainFields[1]?1280:720;
    };
    fixture.object_get_class=[](Il2CppObject*){return &texturesClass;};
    fixture.class_get_method_from_name=[](Il2CppClass*,const char*,int)->const MethodInfo*{return &getter;};
    fixture.string_new=[](const char* s)->Il2CppString*{assert(std::string(s)=="ExampleInvasion  50% (5/10)");return reinterpret_cast<Il2CppString*>(&assetObject);};
    fixture.runtime_invoke=[](const MethodInfo* m,void*,void** args,Il2CppObject** error)->Il2CppObject* {
        *error=nullptr;
        if(m==&getter)return &textureObject;
        if(m==&rectDraw){
            const auto* rect=static_cast<int32_t*>(args[1]);assert(rect[2]>0 && rect[3]>0);
            const auto* color=static_cast<uint8_t*>(args[2]);
            // Deliberately B,G,R,A memory order; no assumed packed RGBA ABI.
            assert(color[3]==210 || color[3]==245);++rectangles;
        } else {assert(m==&textDraw);const auto* color=static_cast<uint8_t*>(args[3]);
            assert(color[0]==190 && color[1]==240 && color[2]==255 && color[3]==255);++texts;}
        return nullptr;
    };
    using namespace bl::csharp;
    setNativeInvasionHudWorldState(true,1);pumpNativeInvasionHudOnGameThread();assert(hooks==1);
    assert(bunny_invasion_hud_publish(1,"example-uid","ExampleInvasion",1,5,10));
    drawHud(nullptr);assert(rectangles==3 && texts==1 && missingClassLookups==0);
    // Also support the alternate namespace without guessing classes or logging a false error.
    refs()={};g_installed=false;colorClass.ns="Microsoft.Xna.Framework";
    for(auto& f:channels)f.offset-=sizeof(Il2CppObject);
    pumpNativeInvasionHudOnGameThread();assert(hooks==2);drawHud(nullptr);assert(texts==2);
    // Reject inconsistent method contracts and malformed value layouts before hooking/drawing.
    refs()={};g_installed=false;textDraw.params[3].cls=&otherColorClass;
    pumpNativeInvasionHudOnGameThread();assert(hooks==2 && !refs().valid);
    textDraw.params[3].cls=&colorClass;colorClass.size=8;
    pumpNativeInvasionHudOnGameThread();assert(hooks==2 && !refs().valid);
    colorClass.size=4;channels[1].offset=channels[0].offset;
    pumpNativeInvasionHudOnGameThread();assert(hooks==2 && !refs().valid);
    assert(missingClassLookups==0);
    std::cout<<"INVASION_HUD_PRODUCTION_BIND_AND_DRAW_PASS androidGraphicsColor=Drawn alternateColor=Drawn invalidAbi=Rejected missingLookups=0\n";
}
