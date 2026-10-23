#include "experiments/csharp/NativeTooltipPanelColors.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

struct Il2CppType {Il2CppClass* klass;};
struct FieldInfo {const char* name;Il2CppType kind;size_t offset;uint32_t flags;};
struct MethodInfo {const char* name;Il2CppType returned;bool instance;};
struct Il2CppClass {
    const char* ns;const char* name;bool value;int size;
    std::vector<FieldInfo*> fields;
    const MethodInfo* getter;
};
struct Node {Il2CppObject obj;uint8_t values[16]{};};
struct Box {Il2CppObject obj;uint8_t rgba[4]{};};
static Il2CppClass byte{"System","Byte",true,1,{},nullptr};
static Il2CppClass color{"Microsoft.Xna.Framework","Color",true,4,{},nullptr};
static Il2CppClass settings{"","Settings",false,32,{},nullptr};
static Il2CppClass backing{"","TooltipBacking",false,32,{},nullptr};
static Il2CppClass wrong{"System","String",false,32,{},nullptr};
static FieldInfo r{"R",{&byte},2,0},g{"G",{&byte},1,0};
static FieldInfo b{"B",{&byte},0,0},a{"A",{&byte},3,0};
static FieldInfo background{"BackingColor",{&color},sizeof(Il2CppObject),0};
static FieldInfo border{"BorderColor",{&color},sizeof(Il2CppObject)+4,0};
static MethodInfo shopGetter{"get_ShopTooltipBacking",{&color},true};
static bl::il2cpp::Api fake;
static Box shop{{&color,nullptr},{14,24,34,220}};
static int invoked=0;
static bool managedError=false;
namespace bl::il2cpp {
Api& api(){return fake;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(!cls)return nullptr;
    for(auto* f:cls->fields)if(name==f->name)return f;
    return nullptr;
}
}

int main() {
    using namespace bl::csharp;
    color.fields={&r,&g,&b,&a};
    backing.fields={&background,&border};
    settings.getter=&shopGetter;
    fake.class_get_namespace=[](Il2CppClass* cls){return cls->ns;};
    fake.class_get_name=[](Il2CppClass* cls){return cls->name;};
    fake.class_from_il2cpp_type=[](const Il2CppType* type){return type->klass;};
    fake.class_is_valuetype=[](const Il2CppClass* cls){return cls->value;};
    fake.class_value_size=[](Il2CppClass* cls,uint32_t* align){*align=1;return cls->size;};
    fake.class_get_fields=[](Il2CppClass* cls,void** it)->FieldInfo* {
        const size_t i=reinterpret_cast<uintptr_t>(*it);
        if(i>=cls->fields.size())return nullptr;
        *it=reinterpret_cast<void*>(i+1);
        return cls->fields[i];
    };
    fake.field_get_flags=[](FieldInfo* f){return f->flags;};
    fake.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->kind;};
    fake.field_get_offset=[](FieldInfo* f){return f->offset;};
    fake.class_instance_size=[](Il2CppClass*){return uint32_t{sizeof(Node)};};
    fake.class_get_method_from_name=[](Il2CppClass* cls,const char* name,int argc)->const MethodInfo* {
        return cls->getter&&argc==0&&std::strcmp(cls->getter->name,name)==0?cls->getter:nullptr;
    };
    fake.method_get_param_count=[](const MethodInfo*){return uint32_t{0};};
    fake.method_is_instance=[](const MethodInfo* m){return m->instance;};
    fake.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{return &m->returned;};
    fake.object_get_class=[](Il2CppObject* obj){return obj->klass;};
    fake.class_is_assignable_from=[](Il2CppClass* cls,Il2CppClass* target){return cls==target;};
    fake.runtime_invoke=[](const MethodInfo* method,void* receiver,void**,Il2CppObject** error)->Il2CppObject* {
        assert(method==&shopGetter&&receiver!=nullptr);
        ++invoked;
        *error=managedError?static_cast<Il2CppObject*>(receiver):nullptr;
        return &shop.obj;
    };

    Node options{{&settings,nullptr}};
    Node surface{{&backing,nullptr}};
    const uint8_t original[8]={8,18,28,238,60,70,80,180};
    std::memcpy(surface.values,original,8);
    std::array<uint8_t,4> tint{},stroke{};
    NativeTooltipPanelColors pipeline;
    std::string error;
    assert(pipeline.bind(&settings,&backing,0,0,&error));
    assert(pipeline.tintSource==NativeTooltipBackingColorSource::TooltipBacking);
    assert(pipeline.read(&options.obj,&surface.obj,&tint,&stroke));
    assert((tint==std::array<uint8_t,4>{28,18,8,238}));
    assert((stroke==std::array<uint8_t,4>{80,70,60,180}));
    assert(invoked==0);

    assert(pipeline.bind(&settings,&backing,15,0,&error));
    assert(pipeline.tintSource==NativeTooltipBackingColorSource::Settings);
    assert(pipeline.read(&options.obj,&surface.obj,&tint,&stroke));
    assert((tint==std::array<uint8_t,4>{34,24,14,220}));
    assert(invoked==1);
    const auto previous=tint;
    options.obj.klass=&wrong;
    assert(!pipeline.read(&options.obj,&surface.obj,&tint,&stroke));
    assert(tint==previous);
    options.obj.klass=&settings;
    managedError=true;
    assert(!pipeline.read(&options.obj,&surface.obj,&tint,&stroke));
    assert(tint==previous);
    managedError=false;
    shop.obj.klass=&wrong;
    assert(!pipeline.read(&options.obj,&surface.obj,&tint,&stroke));
    shop.obj.klass=&color;
    assert(pipeline.read(&options.obj,&surface.obj,&tint,&stroke));

    auto reject=[&] {
        NativeTooltipPanelColors other;
        assert(!other.bind(&settings,&backing,0,0,&error));
    };
    background.flags=0x10;reject();background.flags=0;
    background.offset=400;reject();background.offset=sizeof(Il2CppObject);
    background.kind.klass=&wrong;reject();background.kind.klass=&color;
    g.offset=2;reject();g.offset=1;
    color.size=8;reject();color.size=4;
    border.kind.klass=&wrong;reject();border.kind.klass=&color;
    auto* getter=shopGetter.name;
    shopGetter.name="get_Another";assert(!pipeline.bind(&settings,&backing,15,0,&error));
    shopGetter.name=getter;
    assert(pipeline.bind(&settings,&backing,15,0,&error));
    std::cout<<"CSHARP_TOOLTIP_COLORS=PASS managedGetterCalls="<<invoked<<"\n";
}
