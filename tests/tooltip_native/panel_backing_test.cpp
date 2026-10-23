#include "experiments/csharp/NativeTooltipPanelBacking.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType type; size_t offset; uint32_t flags=0; };
struct MethodInfo {
    const char* name;
    bool instance=false;
    std::vector<Il2CppType> types;
    std::vector<const char*> names;
    Il2CppType result;
};
struct Il2CppClass {
    const char* ns;const char* name;int bytes;bool value;
    std::vector<FieldInfo*> fields;
    std::vector<MethodInfo*> methods;
};
static Il2CppClass plain{"System","Void",0,true,{},{}};
static Il2CppClass number{"System","Single",4,true,{},{}};
static Il2CppClass integer{"System","Int32",4,true,{},{}};
static Il2CppClass boolean{"System","Boolean",1,true,{},{}};
static Il2CppClass panel{"Terraria.UI","GUIPanel",0,false,{},{}};
static Il2CppClass vec{"Microsoft.Xna.Framework","Vector2",8,true,{},{}};
static Il2CppClass rect{"Microsoft.Xna.Framework","Rectangle",16,true,{},{}};
static Il2CppClass nullable{"System","Nullable`1",20,true,{},{}};
static Il2CppClass color{"Microsoft.Xna.Framework","Color",4,true,{},{}};
static Il2CppClass texture{"Microsoft.Xna.Framework.Graphics","Texture2D",0,false,{},{}};
static Il2CppClass unrelated{"System","String",0,false,{},{}};
static Il2CppClass byte{"System","Byte",1,true,{},{}};
static FieldInfo vecX{"X",{&number},0},vecY{"Y",{&number},4};
static FieldInfo colorR{"R",{&byte},0},colorG{"G",{&byte},1};
static FieldInfo colorB{"B",{&byte},2},colorA{"A",{&byte},3};
static FieldInfo rx{"X",{&integer},0},ry{"Y",{&integer},4};
static FieldInfo rw{"Width",{&integer},8},rh{"Height",{&integer},12};
static FieldInfo nf{"hasValue",{&boolean},0},nv{"value",{&rect},4};
static MethodInfo method{"DrawBacking",false,
    {{&vec},{&vec},{&texture},{&color},{&nullable}},
    {"position","size","texture","color","sourceFrame"},{&plain}};
static bl::il2cpp::Api fixture;
static bool throwManaged=false;
static int calls=0;
static float gotX=0,gotY=0,gotW=0,gotH=0;
static uint8_t channels[4]{};

namespace bl::il2cpp {
Api& api(){return fixture;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(!cls)return nullptr;
    for(auto* field:cls->fields)if(name==field->name)return field;
    return nullptr;
}
}

int main() {
    using namespace bl::csharp;
    vec.fields={&vecX,&vecY};
    color.fields={&colorR,&colorG,&colorB,&colorA};
    rect.fields={&rx,&ry,&rw,&rh};
    nullable.fields={&nf,&nv};
    panel.methods={&method};
    fixture.class_get_name=[](Il2CppClass* cls){return cls->name;};
    fixture.class_get_namespace=[](Il2CppClass* cls){return cls->ns;};
    fixture.class_is_valuetype=[](const Il2CppClass* cls){return cls->value;};
    fixture.class_value_size=[](Il2CppClass* cls,uint32_t* align){*align=4;return cls->bytes;};
    fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
    fixture.class_get_fields=[](Il2CppClass* cls,void** it)->FieldInfo*{
        const auto i=reinterpret_cast<uintptr_t>(*it);
        if(i>=cls->fields.size())return nullptr;
        *it=reinterpret_cast<void*>(i+1);
        return cls->fields[i];
    };
    fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
    fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
    fixture.class_get_methods=[](Il2CppClass* cls,void** it)->const MethodInfo*{
        const auto i=reinterpret_cast<uintptr_t>(*it);
        if(i>=cls->methods.size())return nullptr;
        *it=reinterpret_cast<void*>(i+1);
        return cls->methods[i];
    };
    fixture.method_get_name=[](const MethodInfo* m){return m->name;};
    fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};
    fixture.method_get_param_count=[](const MethodInfo* m){return static_cast<uint32_t>(m->types.size());};
    fixture.method_get_param=[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return &m->types.at(i);};
    fixture.method_get_param_name=[](const MethodInfo* m,uint32_t i){return m->names.at(i);};
    fixture.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{return &m->result;};
    fixture.object_get_class=[](Il2CppObject* object){return object->klass;};
    fixture.class_is_assignable_from=[](Il2CppClass* base,Il2CppClass* actual){return base==actual;};
    fixture.runtime_invoke=[](const MethodInfo* target,void* self,void** args,Il2CppObject** error)->Il2CppObject*{
        assert(target==&method&&self==nullptr&&args[2]!=nullptr);
        const auto* pos=static_cast<const float*>(args[0]);
        const auto* size=static_cast<const float*>(args[1]);
        gotX=pos[0];gotY=pos[1];gotW=size[0];gotH=size[1];
        std::memcpy(channels,args[3],4);
        const auto* source=static_cast<const uint8_t*>(args[4]);
        for(int i=0;i<20;++i)assert(source[i]==0);
        ++calls;
        *error=throwManaged?reinterpret_cast<Il2CppObject*>(args[2]):nullptr;
        return nullptr;
    };
    NativeTooltipPanelBacking bridge;
    std::string error;
    assert(bridge.bind(&panel,&error));
    assert(bridge.method==&method&&bridge.nullableBytes==20);
    NativeTooltipPanelResult layout;
    layout.opaque=true;
    layout.panelX=10;layout.panelY=30;layout.panelWidth=171;layout.panelHeight=103;
    Il2CppObject graphic{&texture,nullptr},badGraphic{&unrelated,nullptr};
    assert(bridge.draw(layout,&graphic,20,30,40,150));
    assert(calls==1&&gotX==10&&gotY==30&&gotW==171&&gotH==103);
    assert(channels[0]==20&&channels[1]==30&&channels[2]==40&&channels[3]==150);
    assert(!bridge.draw(layout,&badGraphic,1,2,3,4)&&calls==1);
    layout.opaque=false;assert(!bridge.draw(layout,&graphic,1,2,3,4)&&calls==1);
    layout.opaque=true;layout.panelWidth=0;
    assert(!bridge.draw(layout,&graphic,1,2,3,4));
    layout.panelWidth=171;
    throwManaged=true;assert(!bridge.draw(layout,&graphic,1,2,3,4)&&calls==2);throwManaged=false;
    auto mustReject=[&] {
        NativeTooltipPanelBacking rejected;
        assert(!rejected.bind(&panel,&error));
        assert(rejected.method==nullptr);
    };
    method.instance=true;mustReject();method.instance=false;
    method.names[4]="frame";mustReject();method.names[4]="sourceFrame";
    method.result.cls=&integer;mustReject();method.result.cls=&plain;
    auto badName=nullable.name;nullable.name="Unrelated";mustReject();nullable.name=badName;
    const int original=nullable.bytes;nullable.bytes=24;mustReject();nullable.bytes=original;
    nv.offset=8;mustReject();nv.offset=4;
    colorA.offset=4;mustReject();colorA.offset=3;
    vecY.offset=0;mustReject();vecY.offset=4;
    method.types[2].cls=&unrelated;mustReject();method.types[2].cls=&texture;
    auto otherMethod=method;panel.methods.push_back(&otherMethod);mustReject();panel.methods.pop_back();
    assert(bridge.bind(&panel,&error));
    std::cout<<"NATIVE_TOOLTIP_PANEL_BACKING=PASS invoke="<<calls<<" rejects=10\n";
}
