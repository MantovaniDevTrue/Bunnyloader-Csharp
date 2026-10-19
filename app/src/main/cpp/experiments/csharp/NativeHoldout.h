#pragma once
#include "experiments/csharp/NativeDefaults.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include "hook/HookManager.h"

namespace bl::csharp::holdout {
struct Vector {float x,y;};
struct Rectangle {int x,y,width,height;};
struct Entry {std::string mod,name,source;int type;float x,y;};
struct Refs {int itemType=-1,location=-1,direction=-1,gravity=-1,rotation=-1;};
inline Refs refs;
using StyleFn=void(*)(Il2CppObject*,float,Il2CppObject*,Rectangle,const MethodInfo*);
inline StyleFn originalUse=nullptr,originalHold=nullptr;
inline std::shared_ptr<const std::map<int,Vector>> policies=std::make_shared<const std::map<int,Vector>>();
inline std::mutex writer;
inline void apply(Il2CppObject* player,Il2CppObject* item) {
    if(!player||!item)return;
    const auto table=std::atomic_load_explicit(&policies,std::memory_order_acquire);
    if(table->empty())return;
    int type=0;std::memcpy(&type,reinterpret_cast<char*>(item)+refs.itemType,4);
    const auto found=table->find(type);if(found==table->end())return;
    int direction=0;float gravity=0,angle=0;Vector location{};
    std::memcpy(&direction,reinterpret_cast<char*>(player)+refs.direction,4);
    std::memcpy(&gravity,reinterpret_cast<char*>(player)+refs.gravity,4);std::memcpy(&angle,reinterpret_cast<char*>(player)+refs.rotation,4);
    std::memcpy(&location,reinterpret_cast<char*>(player)+refs.location,8);
    if(!std::isfinite(angle)||!std::isfinite(gravity)||!std::isfinite(location.x)||!std::isfinite(location.y))return;
    const float x=found->second.x*direction,y=found->second.y*gravity,c=std::cos(angle),s=std::sin(angle);
    location.x+=x*c-y*s;location.y+=x*s+y*c;
    std::memcpy(reinterpret_cast<char*>(player)+refs.location,&location,8);
}
inline void use(Il2CppObject* player,float mount,Il2CppObject* item,Rectangle frame,const MethodInfo* method){if(originalUse)originalUse(player,mount,item,frame,method);apply(player,item);}
inline void hold(Il2CppObject* player,float mount,Il2CppObject* item,Rectangle frame,const MethodInfo* method){if(originalHold)originalHold(player,mount,item,frame,method);apply(player,item);}
inline bool prepare(std::string* error) {
    if(originalUse&&originalHold)return true;
    using namespace il2cpp;auto* player=findClass({"Terraria","Player",{}});auto* item=findClass({"Terraria","Item",{}});
    auto* rectangle=findClass({"Microsoft.Xna.Framework","Rectangle",{}});ValueLayout layout;
    if(!resolveValueLayout(rectangle,&layout,error)||layout.size!=16){*error="API=BunnyHoldout Stage=BindStyle Expected=Rectangle16 Actual=InvalidValueLayout Detail="+*error;return false;}
    uint32_t rectangleAlignment=0;api().class_value_size(rectangle,&rectangleAlignment);
    if(rectangleAlignment!=alignof(Rectangle)){*error="API=BunnyHoldout Stage=BindStyle Expected=RectangleAlignment"+std::to_string(alignof(Rectangle))+" Actual="+std::to_string(rectangleAlignment)+" Reason=NativeAbiMismatch";return false;}
    const char* names[]={"X","Y","Width","Height"};
    for(size_t i=0;i<4;++i) {
        auto* f=findField(rectangle,names[i]);auto* c=f?api().class_from_il2cpp_type(api().field_get_type(f)):nullptr;uint32_t align=0;size_t offset=0;
        if(!f||!c||(api().field_get_flags(f)&0x10u)||std::strcmp(api().class_get_name(c),"Int32")||std::strcmp(api().class_get_namespace(c),"System")||
           !api().class_is_valuetype(c)||api().class_value_size(c,&align)!=4||!layout.offset(api().field_get_offset(f),4,&offset)||offset!=i*4) {
            *error="API=BunnyHoldout Field=Rectangle."+std::string(names[i])+" Stage=BindStyle Expected=SequentialInt32 Actual=LayoutMismatch Reason=NativeAbiMismatch";return false;
        }
    }
    Refs staged;
    const auto bind=[&](Il2CppClass* cls,const char* name,const char* type,size_t width,int* offset){FieldInfo* field=nullptr;
        if(!nativeFieldContract(cls,name,type,false,width,&field,offset,error))return false;
        if(api().field_get_flags(field)&0x20u){*error="API=BunnyHoldout Field="+std::string(name)+" Stage=BindStyle Expected=MutableNativeField Actual=InitOnly Reason=NativeStorageMismatch";return false;}
        return true;};
    if(!bind(item,"type","System.Int32",4,&staged.itemType)||!bind(player,"itemLocation","Microsoft.Xna.Framework.Vector2",8,&staged.location)||
       !bind(player,"direction","System.Int32",4,&staged.direction)||!bind(player,"gravDir","System.Single",4,&staged.gravity)||!bind(player,"itemRotation","System.Single",4,&staged.rotation))return false;
    auto* useMethod=findMethodBySignature(player,parseSignature("void ItemCheck_ApplyUseStyle(float mountOffset, Item sItem, Rectangle heldItemFrame)"));
    auto* holdMethod=findMethodBySignature(player,parseSignature("void ItemCheck_ApplyHoldStyle(float mountOffset, Item sItem, Rectangle heldItemFrame)"));
    if(!useMethod||!holdMethod||!api().method_is_instance(useMethod)||!api().method_is_instance(holdMethod)) {
        *error="API=BunnyHoldout Stage=BindStyle Expected=InstanceUseStyleAndHoldStyle(Single,Item,Rectangle) Actual=MissingOrWrongSignature Reason=NativeMethodMismatch";return false;
    }
    refs=staged;
    if((!originalUse&&!hook::install(useMethod,use,&originalUse))||(!originalHold&&!hook::install(holdMethod,hold,&originalHold))){*error="API=BunnyHoldout Stage=InstallStyles Reason=NativeHookInstallFailed";return false;}
    return true;
}
inline bool publish(const std::vector<Entry>& entries,std::string* error) {
    if(entries.empty())return true;
    std::lock_guard lock(writer);
    if(!prepare(error)){
        *error="[CSharp][ERROR][Holdout] Mod="+entries.front().mod+" Class="+entries.front().name+" File="+entries.front().source+" Result=BatchRejected Detail="+*error;return false;
    }
    auto next=std::make_shared<std::map<int,Vector>>(*std::atomic_load_explicit(&policies,std::memory_order_acquire));
    std::map<int,bool> seen;
    for(const auto& entry:entries) {
        if(entry.type<0||!seen.emplace(entry.type,true).second||!std::isfinite(entry.x)||!std::isfinite(entry.y)||std::fabs(entry.x)>1024||std::fabs(entry.y)>1024){*error="[CSharp][ERROR][Holdout] Mod="+entry.mod+" Class="+entry.name+" File="+entry.source+" Stage=Publish Expected=UniqueItemWithFiniteOffsets Result=BatchRejected Reason=InvalidHoldoutDescriptor";return false;}
        (*next)[entry.type]={entry.x,entry.y};
    }
    std::atomic_store_explicit(&policies,std::shared_ptr<const std::map<int,Vector>>(std::move(next)),std::memory_order_release);return true;
}
}
