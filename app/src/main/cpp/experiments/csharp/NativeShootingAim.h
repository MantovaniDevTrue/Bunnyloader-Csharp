#pragma once
#include "experiments/csharp/NativeStaticValue.h"
#include "il2cpp/FieldContract.h"
#include <cmath>

namespace bl::csharp::shooting {
// All metadata is resolved once. Per shot: local identity, center and current aim only.
struct AimBinding {
    int position=-1,width=-1,direction=-1;
    native_static::Value mouse,localPlayer;

    bool bind(Il2CppClass* player,Il2CppClass* main,std::string* error) {
        using namespace il2cpp;
        auto& a=api();AimBinding staged;FieldInfo* positionField=nullptr,*directionField=nullptr;
        if(!nativeFieldContract(player,"position","Microsoft.Xna.Framework.Vector2",false,8,&positionField,&staged.position,error)||
           !nativeFieldContract(player,"width","System.Int32",false,4,nullptr,&staged.width,error)||
           !nativeFieldContract(player,"direction","System.Int32",false,4,&directionField,&staged.direction,error))return false;
        if(a.field_get_flags(directionField)&(0x20u|0x40u)) {
            *error="Field=Player.direction Reason=ReadOnlyNativeAimField";return false;
        }
        auto* vector=a.class_from_il2cpp_type(a.field_get_type(positionField));
        staged.mouse=native_static::resolve(main,"MouseWorld");
        // Android exposes myPlayer through get_myPlayer, as in NativeCommandChatHooks.
        staged.localPlayer=native_static::resolve(main,"myPlayer");
        auto* localType=staged.localPlayer.type;
        const char* localNs=localType?a.class_get_namespace(localType):nullptr;
        const char* localName=localType?a.class_get_name(localType):nullptr;
        if(!staged.localPlayer||!localNs||!localName||std::strcmp(localNs,"System")||
           std::strcmp(localName,"Int32")||!a.class_is_valuetype(localType)||native_static::size(staged.localPlayer)!=4) {
            *error="FieldOrProperty=Main.myPlayer Expected=StaticInt32 Reason=NativeLocalPlayerContractMismatch";return false;
        }
        // X/Y may use raw or boxed offsets; never assume that size alone proves layout.
        auto* x=findField(vector,"X");auto* y=findField(vector,"Y");
        auto scalar=[&](FieldInfo* field) {
            if(!field||(a.field_get_flags(field)&0x10u))return false;
            auto* type=a.class_from_il2cpp_type(a.field_get_type(field));uint32_t align=0;
            const char* ns=type?a.class_get_namespace(type):nullptr,*name=type?a.class_get_name(type):nullptr;
            return ns&&name&&!std::strcmp(ns,"System")&&!std::strcmp(name,"Single")&&
                a.class_is_valuetype(type)&&a.class_value_size(type,&align)==4;
        };
        if(!scalar(x)||!scalar(y)||!staged.mouse||staged.mouse.type!=vector||native_static::size(staged.mouse)!=8||
           !((a.field_get_offset(x)==0&&a.field_get_offset(y)==4)||
             (a.field_get_offset(x)==sizeof(Il2CppObject)&&a.field_get_offset(y)==sizeof(Il2CppObject)+4))) {
            *error="Field=Main.MouseWorld/Player.position Expected=Vector2XYSingleLayout Reason=NativeAimLayoutMismatch";return false;
        }
        *this=staged;return true;
    }

    bool synchronize(Il2CppObject* player,int who) const {
        if(!player||position<0)return false;
        int local=-1;float aim[2]{},pos[2]{};int bodyWidth=0;
        if(!native_static::read(localPlayer,&local,sizeof local)||local<0||local>255)return false;
        if(who!=local)return true; // Remote players retain their synchronized network direction.
        if(!native_static::read(mouse,aim,sizeof aim))return false;
        std::memcpy(pos,reinterpret_cast<char*>(player)+position,sizeof pos);
        std::memcpy(&bodyWidth,reinterpret_cast<char*>(player)+width,sizeof bodyWidth);
        if(bodyWidth<1||bodyWidth>4096)return false;
        for(float n:{aim[0],aim[1],pos[0],pos[1]})if(!std::isfinite(n)||std::fabs(n)>1e8f)return false;
        const float centerX=pos[0]+bodyWidth*0.5f;
        if(aim[0]==centerX)return true; // A vertical shot keeps the current facing.
        const int facing=aim[0]<centerX?-1:1;
        std::memcpy(reinterpret_cast<char*>(player)+direction,&facing,sizeof facing);return true;
    }
};
}
