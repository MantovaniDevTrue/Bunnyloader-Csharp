#pragma once
#include "il2cpp/FieldContract.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace bl::csharp {
struct SummonVector { float x=0,y=0; };
struct SummonMovement {
    bool enabled=false;
    float followSpeed=6,attackSpeed=10,catchSpeed=12,followInertia=18,attackInertia=12;
    float offsetX=-42,offsetY=-46,catchDistance=600,idleDistance=20,idleDamping=.82f;
    float catchInertia=60,teleportDistance=2000,spacing=40,attackStop=40,ownerLeash=1200,rotation=.05f,separation=.04f;
};
inline bool parseSummonMovement(const std::string& text,SummonMovement* output,std::string* error) {
    if(text.empty()||text.back()==','){*error="API=BunnyProjectile.MinionMovement Reason=InvalidMovementDescriptor";return false;}
    std::array<float,17> values{};std::istringstream stream(text);std::string part;
    for(auto& value:values) {
        if(!std::getline(stream,part,',')||part.empty()||part.size()>24) { *error="API=BunnyProjectile.MinionMovement Reason=InvalidMovementDescriptor Expected=17FiniteSingles";return false; }
        char* end=nullptr;value=std::strtof(part.c_str(),&end);
        if(!end||*end||!std::isfinite(value)){*error="API=BunnyProjectile.MinionMovement Reason=InvalidNumericValue Actual="+part;return false;}
    }
    if(std::getline(stream,part,',')){*error="API=BunnyProjectile.MinionMovement Reason=UnexpectedMovementField";return false;}
    const float low[]={.01f,.01f,.01f,1,1,-2000,-2000,1,0,0,1,1,0,0,1,-1,0};
    const float high[]={40,40,40,1000,1000,2000,2000,4000,4000,1,1000,10000,200,400,4000,1,1};
    for(size_t i=0;i<values.size();++i)if(values[i]<low[i]||values[i]>high[i]) {
        *error="API=BunnyProjectile.MinionMovement FieldIndex="+std::to_string(i)+" Expected="+std::to_string(low[i])+".."+std::to_string(high[i])+" Actual="+std::to_string(values[i])+" Reason=OutOfRange";return false;
    }
    if(values[8]>values[7]||values[11]<=values[7]){*error="API=BunnyProjectile.MinionMovement Reason=InconsistentDistances";return false;}
    *output={true,values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9],values[10],values[11],values[12],values[13],values[14],values[15],values[16]};return true;
}
struct SummonOwnerFields {int active=-1,dead=-1,buffTypes=-1,buffTimes=-1,position=-1,width=-1,height=-1,direction=-1,manualTarget=-1;};
inline bool bindSummonOwnerFields(Il2CppClass* cls,SummonOwnerFields* output,std::string* error) {
    SummonOwnerFields staged;
    auto bind=[&](const char* name,const char* type,size_t size,int* field){return il2cpp::nativeFieldContract(cls,name,type,false,size,nullptr,field,error);};
    if(!bind("active","System.Boolean",1,&staged.active)||!bind("dead","System.Boolean",1,&staged.dead)||
       !bind("buffType","System.Int32[]",sizeof(void*),&staged.buffTypes)||!bind("buffTime","System.Int32[]",sizeof(void*),&staged.buffTimes)||
       !bind("position","Microsoft.Xna.Framework.Vector2",8,&staged.position)||!bind("width","System.Int32",4,&staged.width)||
       !bind("height","System.Int32",4,&staged.height)||!bind("direction","System.Int32",4,&staged.direction))return false;
    if(il2cpp::findField(cls,"MinionAttackTargetNPC")&&!bind("MinionAttackTargetNPC","System.Int32",4,&staged.manualTarget))return false;
    *output=staged;return true;
}
template<class T> inline T& summonField(Il2CppObject* object,int offset) {return *reinterpret_cast<T*>(reinterpret_cast<char*>(object)+offset);}
enum class SummonLife { InvalidState, OwnerInactive, OwnerDead, BuffCancelled, Alive, StartBuff };
// Reusable, native memory path. Never infer ownership from LocalPlayer or projectile counts.
inline SummonLife summonLife(Il2CppObject* owner,const SummonOwnerFields& fields,int buff,bool initial) {
    if(!owner||!summonField<uint8_t>(owner,fields.active))return SummonLife::OwnerInactive;
    if(summonField<uint8_t>(owner,fields.dead))return SummonLife::OwnerDead;
    auto* types=summonField<Il2CppArray*>(owner,fields.buffTypes);auto* times=summonField<Il2CppArray*>(owner,fields.buffTimes);
    if(buff<=0||!il2cpp::nativeArrayContract(types,"Int32",4,60)||!il2cpp::nativeArrayContract(times,"Int32",4,60)||types->length!=times->length)return SummonLife::InvalidState;
    const auto* ids=static_cast<const int32_t*>(arrayData(types));const auto* ticks=static_cast<const int32_t*>(arrayData(times));
    for(size_t i=0;i<types->length;++i)if(ids[i]==buff&&ticks[i]>0)return SummonLife::Alive;
    return initial?SummonLife::StartBuff:SummonLife::BuffCancelled;
}
inline float summonDistanceSquared(SummonVector a,SummonVector b) {float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
inline SummonVector summonIdle(const SummonMovement& p,SummonVector owner,int direction,int rank) {
    return {owner.x+(p.offsetX+p.spacing*rank)*(direction<0?1.f:-1.f),owner.y+p.offsetY};
}
inline SummonVector steerSummon(const SummonMovement& p,SummonVector position,SummonVector velocity,SummonVector destination,bool attack) {
    const float squared=summonDistanceSquared(position,destination);
    if(!std::isfinite(squared)||!std::isfinite(velocity.x)||!std::isfinite(velocity.y))return {};
    if(attack&&squared<=p.attackStop*p.attackStop)return velocity;
    if(!attack&&squared<=p.idleDistance*p.idleDistance)return {velocity.x*p.idleDamping,velocity.y*p.idleDamping};
    const float distance=std::sqrt(squared);if(distance<.001f)return velocity;
    const bool far=!attack&&distance>p.catchDistance;
    const float speed=attack?p.attackSpeed:far?p.catchSpeed:p.followSpeed;
    const float inertia=attack?p.attackInertia:far?p.catchInertia:p.followInertia;
    return {(velocity.x*(inertia-1)+(destination.x-position.x)*speed/distance)/inertia,
            (velocity.y*(inertia-1)+(destination.y-position.y)*speed/distance)/inertia};
}
inline int summonSourceDamage(int existing,int itemDamage,int projectileDamage) {
    if(existing>0)return existing;
    if(itemDamage>=0&&itemDamage<=1000000)return itemDamage;
    return projectileDamage>0&&projectileDamage<=1000000?projectileDamage:0;
}
}
