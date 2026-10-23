#include "experiments/csharp/NativeTooltipPanelTexturePath.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Il2CppType {Il2CppClass* klass;};
struct FieldInfo {const char* name;Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {const char* name;Il2CppType result;bool instance=true;};
struct Il2CppClass {
    const char* ns;const char* name;bool value=false;
    std::vector<FieldInfo*> fields;
    const MethodInfo* getter=nullptr;
};
struct Node {
    Il2CppObject header;
    Il2CppObject* first=nullptr;
    Il2CppObject* second=nullptr;
};
static Il2CppClass settings{"","Settings",false,{},nullptr},tips{"","TooltipSettings",false,{},nullptr};
static Il2CppClass backing{"","TooltipBacking",false,{},nullptr},imageSlot{"","TooltipImageLayout",false,{},nullptr};
static Il2CppClass texture{"Microsoft.Xna.Framework.Graphics","Texture2D",false,{},nullptr};
static Il2CppClass wrong{"System","String",false,{},nullptr},primitive{"System","Int32",true,{},nullptr};
static FieldInfo settingsTips{"Tooltips",{&tips},sizeof(Il2CppObject)};
static FieldInfo tipsBacking{"Backing",{&backing},sizeof(Il2CppObject)};
static FieldInfo backingFill{"Backing",{&imageSlot},sizeof(Il2CppObject)};
static FieldInfo backingBorder{"Border",{&imageSlot},sizeof(Il2CppObject)+sizeof(void*)};
static MethodInfo textureGetter{"get_Texture",{&texture},true};
static bl::il2cpp::Api fixture;
static bool getterFailure=false;
static int getterCalls=0;

namespace bl::il2cpp {
Api& api(){return fixture;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(!cls)return nullptr;
    for(auto* f:cls->fields)if(name==f->name)return f;
    return nullptr;
}
}

int main() {
    using namespace bl::csharp;
    settings.fields={&settingsTips};
    tips.fields={&tipsBacking};
    backing.fields={&backingFill,&backingBorder};
    imageSlot.getter=&textureGetter;
    fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    fixture.class_get_name=[](Il2CppClass* c){return c->name;};
    fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    fixture.class_get_method_from_name=[](Il2CppClass* c,const char* name,int count)->const MethodInfo* {
        return c->getter&&std::string(name)==c->getter->name&&count==0?c->getter:nullptr;
    };
    fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};
    fixture.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{return &m->result;};
    fixture.method_get_param_count=[](const MethodInfo*){return uint32_t{0};};
    fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->klass;};
    fixture.class_is_assignable_from=[](Il2CppClass* expected,Il2CppClass* actual){return expected==actual;};
    fixture.object_get_class=[](Il2CppObject* obj){return obj->klass;};
    fixture.field_get_flags=[](FieldInfo* field){return field->flags;};
    fixture.field_get_type=[](FieldInfo* field)->const Il2CppType*{return &field->type;};
    fixture.field_get_offset=[](FieldInfo* field){return field->offset;};
    fixture.class_instance_size=[](Il2CppClass*){return static_cast<uint32_t>(sizeof(Node));};
    fixture.runtime_invoke=[](const MethodInfo* m,void* parent,void**,Il2CppObject** error)->Il2CppObject* {
        assert(m==&textureGetter);
        ++getterCalls;
        *error=getterFailure?static_cast<Il2CppObject*>(parent):nullptr;
        return static_cast<Node*>(parent)->first;
    };

    Node fillImage{{&imageSlot,nullptr}};
    Node borderImage{{&imageSlot,nullptr}};
    Node fillTex{{&texture,nullptr}};
    Node borderTex{{&texture,nullptr}};
    Node backingValue{{&backing,nullptr}};
    Node tipsValue{{&tips,nullptr}};
    Node settingsValue{{&settings,nullptr}};
    fillImage.first=&fillTex.header;
    borderImage.first=&borderTex.header;
    backingValue.first=&fillImage.header;
    backingValue.second=&borderImage.header;
    tipsValue.first=&backingValue.header;
    settingsValue.first=&tipsValue.header;

    NativeTooltipPanelTexturePath path;
    std::string error;
    assert(path.bind(&settings,&error));
    Il2CppObject *a=nullptr,*b=nullptr;
    assert(path.read(&settingsValue.header,&a,&b));
    assert(a==&fillTex.header&&b==&borderTex.header&&getterCalls==2);

    const int calls=getterCalls;
    Il2CppObject* sentinel=reinterpret_cast<Il2CppObject*>(uintptr_t{0x1234});
    a=sentinel;b=sentinel;
    assert(!path.read(&tipsValue.header,&a,&b));
    assert(getterCalls==calls&&a==sentinel&&b==sentinel);
    borderImage.first=nullptr;
    assert(!path.read(&settingsValue.header,&a,&b));
    assert(a==sentinel&&b==sentinel);
    borderImage.first=&borderTex.header;
    getterFailure=true;
    assert(!path.read(&settingsValue.header,&a,&b));
    assert(a==sentinel&&b==sentinel);
    getterFailure=false;
    borderTex.header.klass=&wrong;
    assert(!path.read(&settingsValue.header,&a,&b));
    borderTex.header.klass=&texture;
    assert(path.read(&settingsValue.header,&a,&b));
    assert(a==&fillTex.header&&b==&borderTex.header);

    auto reject=[&] {
        NativeTooltipPanelTexturePath other;
        assert(!other.bind(&settings,&error));
    };
    settingsTips.flags=0x10;reject();settingsTips.flags=0;
    settingsTips.offset=sizeof(Node)+8;reject();settingsTips.offset=sizeof(Il2CppObject);
    tipsBacking.type.klass=&primitive;reject();tipsBacking.type.klass=&backing;
    backingBorder.type.klass=&wrong;reject();backingBorder.type.klass=&imageSlot;
    textureGetter.instance=false;reject();textureGetter.instance=true;
    textureGetter.result.klass=&wrong;reject();textureGetter.result.klass=&texture;
    imageSlot.getter=nullptr;reject();imageSlot.getter=&textureGetter;
    assert(path.read(&settingsValue.header,&a,&b));
    std::cout<<"CSHARP_TOOLTIP_TEXTURE_PATH=PASS getterCalls="<<getterCalls<<" rejected=7\n";
}
