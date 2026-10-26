#include "experiments/csharp/NativeTooltipPanelRenderer.h"
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
static Il2CppClass panel{"","GUIPanel",0,false,{},{}};
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
static MethodInfo multiply{"Multiply",false,{{&color},{&number}},{"color","scale"},{&color}};
static MethodInfo method{"DrawBacking",false,
    {{&vec},{&vec},{&texture},{&color},{&nullable}},
    {"position","size","texture","color","sourceFrame"},{&plain}};
struct Node {
    Il2CppObject object;
    Il2CppObject* first=nullptr;
    Il2CppObject* second=nullptr;
    uint8_t rgba[8]{};
};
static Il2CppClass settings{"","Settings",0,false,{},{}};
static Il2CppClass tooltips{"","TooltipSettings",0,false,{},{}};
static Il2CppClass backingValue{"","TooltipBacking",0,false,{},{}};
static Il2CppClass image{"","TooltipImageLayout",0,false,{},{}};
static FieldInfo rootField{"Instance",{&settings},0,0x10u};
static FieldInfo settingTips{"Tooltips",{&tooltips},sizeof(Il2CppObject)};
static FieldInfo shopTint{"ShopTooltipBacking",{&color},offsetof(Node,rgba)};
static FieldInfo tipsBack{"Backing",{&backingValue},sizeof(Il2CppObject)};
static FieldInfo imageFill{"Backing",{&image},sizeof(Il2CppObject)};
static FieldInfo imageBorder{"Border",{&image},offsetof(Node,second)};
static FieldInfo imageTexture{"Texture",{&texture},sizeof(Il2CppObject)};
static FieldInfo tintDefault{"BackingColor",{&color},offsetof(Node,rgba)};
static FieldInfo tintBorder{"BorderColor",{&color},offsetof(Node,rgba)+4};
static Il2CppObject* settingsSingleton=nullptr;
static bl::il2cpp::Api fixture;
struct BoxedColor {Il2CppObject obj;uint8_t bytes[4];};
static BoxedColor colorResult{{&color,nullptr},{0,0,0,0}};
static int multiplications=0;
static bool multiplyFailure=false;
static bool throwManaged=false;
static int calls=0;
static Il2CppObject* textureReceived=nullptr;
static float gotX=0,gotY=0,gotW=0,gotH=0;
static uint8_t channels[4]{};
static std::vector<Il2CppObject*> renderedTextures;
static std::vector<std::array<uint8_t,4>> renderedColors;

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
    color.methods={&multiply};
    rect.fields={&rx,&ry,&rw,&rh};
    nullable.fields={&nf,&nv};
    panel.methods={&method};
    settings.fields={&rootField,&settingTips,&shopTint};
    tooltips.fields={&tipsBack};
    backingValue.fields={&imageFill,&imageBorder,&tintDefault,&tintBorder};
    image.fields={&imageTexture};
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
    fixture.class_instance_size=[](Il2CppClass*){return static_cast<uint32_t>(sizeof(Node));};
    fixture.field_static_get_value=[](FieldInfo* field,void* output){
        assert(field==&rootField);
        std::memcpy(output,&settingsSingleton,sizeof(settingsSingleton));
    };
    fixture.class_get_method_from_name=[](Il2CppClass* cls,const char* name,int count)->const MethodInfo*{
        for(auto* item:cls->methods)
            if(std::strcmp(item->name,name)==0&&static_cast<int>(item->types.size())==count)
                return item;
        return nullptr;
    };
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
        if(target==&multiply) {
            assert(self==nullptr&&args&&args[0]&&args[1]);
            float alpha=0;
            std::memcpy(&alpha,args[1],sizeof(alpha));
            const auto* input=static_cast<const uint8_t*>(args[0]);
            assert(alpha>=0.f&&alpha<=1.f);
            for(int i=0;i<4;++i)
                colorResult.bytes[i]=static_cast<uint8_t>(static_cast<float>(input[i])*alpha);
            *error=multiplyFailure?&colorResult.obj:nullptr;
            ++multiplications;
            return &colorResult.obj;
        }
        assert(target==&method&&self==nullptr&&args[2]!=nullptr);
        textureReceived=static_cast<Il2CppObject*>(args[2]);   // IL2CPP: parametro referencia vai como o objeto
        renderedTextures.push_back(textureReceived);
        const auto* pos=static_cast<const float*>(args[0]);
        const auto* size=static_cast<const float*>(args[1]);
        gotX=pos[0];gotY=pos[1];gotW=size[0];gotH=size[1];
        std::memcpy(channels,args[3],4);
        std::array<uint8_t,4> painted{};
        std::memcpy(painted.data(),args[3],4);
        renderedColors.push_back(painted);
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
    assert(textureReceived==&graphic);
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
    auto originalNs=panel.ns;
    panel.ns="Terraria.UI";mustReject();panel.ns=originalNs;
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
    NativeTooltipPanelAlpha alpha;
    assert(alpha.bind(&color,&error));
    std::array<uint8_t,4> tinted{3,4,5,6};
    assert(alpha.apply({20,30,40,200},0.5f,&tinted));
    assert((tinted==std::array<uint8_t,4>{10,15,20,100}));
    assert(multiplications==1);
    const auto tintedBefore=tinted;
    assert(!alpha.apply({20,30,40,200},-0.1f,&tinted));
    assert(!alpha.apply({20,30,40,200},1.1f,&tinted));
    assert(tinted==tintedBefore&&multiplications==1);
    multiply.instance=true;
    NativeTooltipPanelAlpha invalid;
    assert(!invalid.bind(&color,&error));
    multiply.instance=false;
    multiply.types[1].cls=&integer;
    assert(!invalid.bind(&color,&error));
    multiply.types[1].cls=&number;
    auto irrelevant=multiply;
    irrelevant.types[1].cls=&color;
    color.methods.push_back(&irrelevant);
    assert(invalid.bind(&color,&error));
    color.methods.pop_back();
    auto duplicate=multiply;
    color.methods.push_back(&duplicate);
    assert(!invalid.bind(&color,&error));
    color.methods.pop_back();
    assert(invalid.bind(&color,&error));

    Node settingsNode{{&settings,nullptr}};
    Node tooltipsNode{{&tooltips,nullptr}};
    Node backingNode{{&backingValue,nullptr}};
    Node fillNode{{&image,nullptr}};
    Node borderNode{{&image,nullptr}};
    Node fillTexture{{&texture,nullptr}};
    Node borderTexture{{&texture,nullptr}};
    settingsNode.first=&tooltipsNode.object;
    tooltipsNode.first=&backingNode.object;
    backingNode.first=&fillNode.object;
    backingNode.second=&borderNode.object;
    fillNode.first=&fillTexture.object;
    borderNode.first=&borderTexture.object;
    const uint8_t backingColors[8]{14,25,36,200,50,60,70,180};
    std::memcpy(backingNode.rgba,backingColors,8);
    const uint8_t shopColor[4]{3,5,7,240};
    std::memcpy(settingsNode.rgba,shopColor,4);
    settingsSingleton=&settingsNode.object;

    NativeTooltipPanelInspection inspected;
    assert(inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    assert(inspected.resources.settings==&settingsNode.object);
    assert(inspected.resources.tooltipBacking==&backingNode.object);
    assert(inspected.resources.fillTexture==&fillTexture.object);
    assert(inspected.resources.borderTexture==&borderTexture.object);
    assert((inspected.fillRgba==std::array<uint8_t,4>{14,25,36,200}));
    assert((inspected.borderRgba==std::array<uint8_t,4>{50,60,70,180}));
    assert(inspectNativeTooltipPanel(&settings,&panel,15,0,&inspected,&error));
    assert((inspected.fillRgba==std::array<uint8_t,4>{3,5,7,240}));
    auto old=inspected;
    settingsSingleton=nullptr;
    assert(!inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    assert(inspected.resources.settings==old.resources.settings);
    assert(inspected.fillRgba==old.fillRgba);
    settingsSingleton=&settingsNode.object;
    borderNode.first=nullptr;
    assert(!inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    borderNode.first=&borderTexture.object;
    borderTexture.object.klass=&unrelated;
    assert(!inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    borderTexture.object.klass=&texture;
    tintDefault.flags=0x10u;
    assert(!inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    tintDefault.flags=0;
    method.names[2]="otherTexture";
    assert(!inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    method.names[2]="texture";
    assert(inspectNativeTooltipPanel(&settings,&panel,0,0,&inspected,&error));
    assert(inspected.resources.settings==&settingsNode.object);
    NativeTooltipPanelRenderer renderer;
    assert(renderer.bind(&panel,&color,&error));
    NativeTooltipPanelResult bounds;
    bounds.opaque=true;
    bounds.panelX=22;bounds.panelY=40;
    bounds.panelWidth=150;bounds.panelHeight=96;
    const int before=calls;
    const int beforeAlpha=multiplications;
    assert(renderer.draw(bounds,inspected,0.5f));
    assert(calls==before+2&&multiplications==beforeAlpha+2);
    assert(renderedTextures[renderedTextures.size()-2]==&fillTexture.object);
    assert(renderedTextures.back()==&borderTexture.object);
    assert((renderedColors[renderedColors.size()-2]==std::array<uint8_t,4>{7,12,18,100}));
    assert((renderedColors.back()==std::array<uint8_t,4>{25,30,35,90}));
    assert(gotX==22&&gotY==40&&gotW==150&&gotH==96);
    bounds.opaque=false;
    assert(!renderer.draw(bounds,inspected,0.5f)&&calls==before+2);
    bounds.opaque=true;
    assert(!renderer.draw(bounds,inspected,-0.1f)&&calls==before+2);
    auto incomplete=inspected;
    incomplete.resources.borderTexture=nullptr;
    assert(!renderer.draw(bounds,incomplete,0.5f)&&calls==before+2);
    multiplyFailure=true;
    assert(!renderer.draw(bounds,inspected,0.5f)&&calls==before+2);
    multiplyFailure=false;
    throwManaged=true;
    assert(!renderer.draw(bounds,inspected,0.5f)&&calls==before+3);
    throwManaged=false;
    NativeTooltipPanelRenderer mismatch;
    assert(!mismatch.bind(&panel,&unrelated,&error)&&!error.empty());
    std::cout<<"NATIVE_TOOLTIP_PANEL_BACKING=PASS invoke="<<calls
             <<" rejects=10 integrated=PASS colorMultiply=PASS pair=PASS\n";
}
