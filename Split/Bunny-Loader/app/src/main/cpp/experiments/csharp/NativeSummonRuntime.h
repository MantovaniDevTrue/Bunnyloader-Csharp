#pragma once
#include "experiments/csharp/SummonPolicy.h"
#include "experiments/csharp/NativeContent.h"
#include "content/common/GameRefs.h"
#include "core/Log.h"
#include "il2cpp/Signature.h"
#include "il2cpp/GuardedCall.h"

namespace bl::csharp::nativeSummon {
struct SummonRuntime {
    bool ready=false;
    FieldInfo* netMode=nullptr;
    FieldInfo* players=nullptr;
    FieldInfo* projectiles=nullptr;
    Il2CppClass* playerClass=nullptr;
    Il2CppClass* projectileClass=nullptr;
    SummonOwnerFields owner{};
    int friendly=-1, spriteDirection=-1, originalDamage=-1;
    const MethodInfo* addBuff=nullptr;
    const MethodInfo* kill=nullptr;
    const MethodInfo* canChase=nullptr;
    const MethodInfo* canHit=nullptr;
};
inline SummonRuntime g_summon;
// Parameter names can drift between Android releases; storage, return and all
// argument types still match the complete native signature, never name/arity alone.
inline const MethodInfo* summonMethod(Il2CppClass* cls,const char* signature,bool instance) {
    bool ambiguous=false;const MethodInfo* renamed=nullptr;
    auto* found=il2cpp::findMethodBySignature(cls,il2cpp::parseSignature(signature),&ambiguous,&renamed);
    if(!found&&!ambiguous&&renamed) {
        auto actual=il2cpp::parseSignature(il2cpp::describeMethod(renamed));
        found=il2cpp::findMethodBySignature(cls,actual,&ambiguous);
    }
    return found&&!ambiguous&&il2cpp::api().method_is_instance(found)==instance?found:nullptr;
}
inline bool prepareSummon(std::string* error) {
    if(g_summon.ready)return true;
    SummonRuntime staged;auto* main=il2cpp::findClass({"Terraria","Main",{}});
    staged.playerClass=il2cpp::findClass({"Terraria","Player",{}});
    staged.projectileClass=il2cpp::findClass({"Terraria","Projectile",{}});
    if(!il2cpp::nativeFieldContract(main,"netMode","System.Int32",true,4,&staged.netMode,nullptr,error)||
       !bindSummonOwnerFields(staged.playerClass,&staged.owner,error)||
       !il2cpp::nativeFieldContract(main,"player","Terraria.Player[]",true,sizeof(void*),&staged.players,nullptr,error)||
       !il2cpp::nativeFieldContract(main,"projectile","Terraria.Projectile[]",true,sizeof(void*),&staged.projectiles,nullptr,error)||
       !il2cpp::nativeFieldContract(staged.projectileClass,"friendly","System.Boolean",false,1,nullptr,&staged.friendly,error)||
       !il2cpp::nativeFieldContract(staged.projectileClass,"spriteDirection","System.Int32",false,4,nullptr,&staged.spriteDirection,error)||
       !il2cpp::nativeFieldContract(staged.projectileClass,"originalDamage","System.Int32",false,4,nullptr,&staged.originalDamage,error))return false;
    const auto& game=runtime::game();
    struct FieldCheck {const char* name;const char* type;size_t width;int expected;};
    const FieldCheck checks[]={
        {"whoAmI","System.Int32",4,game.entity.whoAmI},{"position","Microsoft.Xna.Framework.Vector2",8,game.entity.position},
        {"velocity","Microsoft.Xna.Framework.Vector2",8,game.entity.velocity},{"width","System.Int32",4,game.entity.width},
        {"height","System.Int32",4,game.entity.height},{"active","System.Boolean",1,game.proj.active},
        {"rotation","System.Single",4,game.proj.rotation},{"type","System.Int32",4,game.proj.type},
        {"owner","System.Int32",4,game.proj.owner},{"timeLeft","System.Int32",4,game.proj.timeLeft},{"damage","System.Int32",4,game.proj.damage}};
    for(const auto& check:checks) {
        int offset=-1;FieldInfo* field=nullptr;
        if(!il2cpp::nativeFieldContract(staged.projectileClass,check.name,check.type,false,check.width,&field,&offset,error))return false;
        if(offset!=check.expected||(il2cpp::api().field_get_flags(field)&0x20u)) {
            *error="Field="+std::string(check.name)+" ExpectedOffset="+std::to_string(check.expected)+" ActualOffset="+std::to_string(offset)+" Reason=NativeContractMismatchOrReadOnly";return false;
        }
    }
    staged.addBuff=summonMethod(staged.playerClass,"void AddBuff(int type, int time, bool fromNetPvP)",true);
    staged.kill=summonMethod(staged.projectileClass,"void Kill()",true);
    staged.canChase=summonMethod(il2cpp::findClass({"Terraria","NPC",{}}),"bool CanBeChasedBy(object attacker, bool ignoreDontTakeDamage)",true);
    staged.canHit=summonMethod(il2cpp::findClass({"Terraria","Collision",{}}),"bool CanHitLine(Vector2 Position1, int Width1, int Height1, Vector2 Position2, int Width2, int Height2)",false);
    if(!staged.addBuff||!staged.kill||!staged.canChase||!staged.canHit) {
        *error="API=BunnyProjectile.Minion Stage=BindMethods Expected=InstanceAddBuff:Int32,Int32,Boolean;InstanceKill;InstanceCanBeChasedBy;StaticCanHitLine Actual="+
            std::string(staged.addBuff?"AddBuff:Present":"AddBuff:MissingOrWrongSignature")+","+(staged.kill?"Kill:Present":"Kill:MissingOrWrongSignature")+","+
            (staged.canChase?"CanBeChasedBy:Present":"CanBeChasedBy:MissingOrWrongSignature")+","+(staged.canHit?"CanHitLine:Present":"CanHitLine:MissingOrWrongSignature")+" Result=Rejected";return false;
    }
    staged.ready=true;g_summon=staged;
    BL_INFO("[CSharp][Summon] API=BunnyProjectile.Minion Stage=BindLifecycle Result=Ready InstanceTracking=NativeSetDefaults");
    return true;
}
inline Il2CppArray* summonObjects(FieldInfo* field,Il2CppClass* element,size_t maximum) {
    Il2CppArray* array=nullptr;il2cpp::api().field_static_get_value(field,&array);auto& api=il2cpp::api();
    return array&&array->length<=maximum&&api.class_get_rank(array->obj.klass)==1&&api.class_get_element_class(array->obj.klass)==element?array:nullptr;
}
inline Il2CppObject* summonOwner(int owner) {
    auto* players=summonObjects(g_summon.players,g_summon.playerClass,256);
    return players&&owner>=0&&owner<255&&static_cast<size_t>(owner)<players->length?static_cast<Il2CppObject**>(arrayData(players))[owner]:nullptr;
}
inline void summonException(const char* method,int type,Il2CppObject* error) {
    static unsigned failures=0;if(failures++<8)BL_ERROR("[CSharp][ERROR][Summon] ContentId=%d Key=%s Method=%s Stage=Invoke Result=OperationRejected Reason=NativeManagedException Exception=%s",
        type,nativeManagedProjectileKey(type).c_str(),method,il2cpp::describeException(error).c_str());
}
inline bool summonBool(const MethodInfo* method,Il2CppObject* object,void** args,int type) {
    Il2CppObject* error=nullptr;auto* result=il2cpp::api().runtime_invoke(method,object,args,&error);
    if(error){summonException("TargetQuery",type,error);return false;}
    if(!result)return false;
    auto* cls=result->klass;uint32_t alignment=0;
    const auto& api=il2cpp::api();
    if(!api.class_is_valuetype(cls)||api.class_value_size(cls,&alignment)!=1||std::strcmp(api.class_get_name(cls),"Boolean")||std::strcmp(api.class_get_namespace(cls),"System"))return false;
    return *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(result)+sizeof(Il2CppObject))!=0;
}
inline void killSummon(Il2CppObject* projectile,int type,SummonLife reason) {
    // Kill invokes the real native cleanup and the existing OnKill bridge once.
    Il2CppObject* error=nullptr;il2cpp::api().runtime_invoke(g_summon.kill,projectile,nullptr,&error);
    if(error)summonException("Projectile.Kill",type,error);
    else {static unsigned samples=0;if(samples++<12)BL_INFO("[CSharp][Summon] Key=%s ContentId=%d Stage=Lifetime Result=Killed Reason=%s",nativeManagedProjectileKey(type).c_str(),type,
        reason==SummonLife::BuffCancelled?"BuffCancelled":reason==SummonLife::OwnerDead?"OwnerDead":reason==SummonLife::OwnerInactive?"OwnerInactive":"InvalidOwnerOrBuffState");}
}

}
