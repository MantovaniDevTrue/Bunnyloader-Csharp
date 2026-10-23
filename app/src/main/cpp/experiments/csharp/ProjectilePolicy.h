#pragma once
#include "il2cpp/FieldContract.h"
#include <cmath>
#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <vector>

namespace bl::csharp {
struct ProjectilePolicy {
    int frames=1, animationFrameTicks=6, minionScanTicks=8;
    float minionTargetRange=700.f;
    bool minion=false;
};
inline bool validateProjectilePolicy(const ProjectilePolicy& p,std::string* error) {
    if(p.frames<1||p.frames>32||p.animationFrameTicks<1||p.animationFrameTicks>120||
       p.minionScanTicks<1||p.minionScanTicks>120||!std::isfinite(p.minionTargetRange)||
       p.minionTargetRange<1||p.minionTargetRange>4000) {
        *error="API=BunnyProjectile Stage=ValidatePolicy Expected=Frames1..32,AnimationFrameTicks1..120,MinionScanTicks1..120,MinionTargetRange1..4000 Actual="+
            std::to_string(p.frames)+","+std::to_string(p.animationFrameTicks)+","+std::to_string(p.minionScanTicks)+","+std::to_string(p.minionTargetRange)+
            " Result=Rejected Reason=ProjectilePolicyOutOfRange";return false;
    }
    return true;
}
inline bool parseProjectileTuning(const std::vector<std::string>& fields,ProjectilePolicy* policy,std::string* error) {
    if(fields.size()!=16&&fields.size()!=17&&fields.size()!=18&&
       fields.size()!=19&&fields.size()!=21){*error="Stage=DecodePolicy Expected=16or17or18or19or21Fields Actual="+std::to_string(fields.size())+" Reason=InvalidDescriptor";return false;}
    auto integer=[](const std::string& text,int* result){
        if(text.empty()||text.size()>11)return false;
        auto parsed=std::from_chars(text.data(),text.data()+text.size(),*result);
        return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size();
    };
    auto staged=*policy;char* end=nullptr;
    if(fields[15].empty()||fields[15].size()>22||
       !integer(fields[13],&staged.animationFrameTicks)||!integer(fields[14],&staged.minionScanTicks)) {
        *error="Stage=DecodePolicy Expected=Int32,Int32,Single Actual="+fields[13]+","+fields[14]+","+fields[15]+" Reason=InvalidNumericValue";return false;
    }
    errno=0;staged.minionTargetRange=std::strtof(fields[15].c_str(),&end);
    if(errno||!end||*end){*error="Field=MinionTargetRange Stage=DecodePolicy Expected=Single Actual="+fields[15]+" Reason=InvalidNumericValue";return false;}
    if(!validateProjectilePolicy(staged,error))return false;
    *policy=staged;return true;
}
inline bool bindProjectileFrameTable(Il2CppClass* main,int type,std::string* error) {
    FieldInfo* field=nullptr;
    if(!il2cpp::nativeFieldContract(main,"projFrames","System.Int32[]",true,sizeof(void*),&field,nullptr,error))return false;
    if(!il2cpp::api().field_static_get_value){*error="Field=projFrames Stage=BindFrames Reason=RequiredExportMissing";return false;}
    Il2CppArray* table=nullptr;il2cpp::api().field_static_get_value(field,&table);
    if(!il2cpp::nativeArrayContract(table,"Int32",4,32768)||type<0||static_cast<uintptr_t>(type)>=table->length) {
        *error="Field=projFrames Stage=BindFrames Expected=Int32ArrayContainingContentId ActualLength="+
            (table?std::to_string(table->length):"Null")+" ContentId="+std::to_string(type)+" Reason=NativeArrayMismatch";return false;
    }
    return true;
}
struct ProjectileGameplayFields {
    int minion=-1,minionSlots=-1,frame=-1,frameCounter=-1;
};
inline bool bindProjectileGameplayFields(Il2CppClass* cls,bool minion,bool animated,
                                        ProjectileGameplayFields* output,std::string* error) {
    ProjectileGameplayFields staged;
    auto bind=[&](const char* name,const char* type,size_t width,int* location) {
        FieldInfo* field=nullptr;
        if(!il2cpp::nativeFieldContract(cls,name,type,false,width,&field,location,error))return false;
        if(il2cpp::api().field_get_flags(field)&0x20u){*error="Field="+std::string(name)+" Expected=WritableInstance Actual=InitOnly Reason=NativeStorageMismatch";return false;}
        return true;
    };
    if(minion&&(!bind("minion","System.Boolean",1,&staged.minion)||
                !bind("minionSlots","System.Single",4,&staged.minionSlots)))return false;
    if(animated&&(!bind("frame","System.Int32",4,&staged.frame)||
                  !bind("frameCounter","System.Int32",4,&staged.frameCounter)))return false;
    *output=staged;return true;
}
inline bool advanceProjectileFrame(Il2CppObject* object,const ProjectileGameplayFields& fields,const ProjectilePolicy& policy) {
    if(!object||policy.frames<=1||fields.frame<static_cast<int>(sizeof(Il2CppObject))||
       fields.frameCounter<static_cast<int>(sizeof(Il2CppObject)))return false;
    int frame=0,counter=0;auto* bytes=reinterpret_cast<char*>(object);
    std::memcpy(&frame,bytes+fields.frame,4);std::memcpy(&counter,bytes+fields.frameCounter,4);
    if(counter<0||counter>=policy.animationFrameTicks-1){counter=0;frame=frame<0||frame>=policy.frames-1?0:frame+1;}
    else ++counter;
    std::memcpy(bytes+fields.frame,&frame,4);std::memcpy(bytes+fields.frameCounter,&counter,4);return true;
}

}
