#include "content/tiles/WaterAlphaTables.h"
#include "content/tiles/ModWater.h"
#include "boot/Boot.h"
#include "il2cpp/PrimitiveArrayTransaction.h"
#include <unistd.h>

namespace bl::runtime {
namespace {
using P=il2cpp::CheckedProperty;using K=P::Kind;
struct WaterTable {
    il2cpp::CheckedFieldWrite field;bool local=false;
    bool bind(Il2CppClass* main,Il2CppClass* localClass,const char* name,std::string* error) {
        if(il2cpp::findField(main,name))return field.bind(main,name,K::SingleArray,true,error);
        P visible;if(!visible.bind(main,name,K::SingleArray,true,error))return false;
        local=true;
        return field.bind(localClass,name,K::SingleArray,false,error) &&
            (visible.declared==field.contract().declared || visible.reject(error,"BindBackingField",P::name(visible.declared),
                P::name(field.contract().declared),"NativeBackingTypeMismatch"));
    }
};
struct AlphaRefs {P local;std::array<WaterTable,2> tables;bool ready=false;} alphaRefs;
bool bindAlphaTables(std::string* error) {
    if(alphaRefs.ready)return true;
    auto* main=il2cpp::findClassQuiet("Terraria","Main");
    auto* local=il2cpp::findClassQuiet("","LocalUserGameState");
    if(!main || !local) {
        if(error)*error="[Native][ERROR][WaterStyle] Stage=BindAlphaTables Expected=Terraria.Main,LocalUserGameState Actual=MissingClasses:"+
            std::string(!main?"Terraria.Main ":"")+(!local?"LocalUserGameState":"")+" Result=Rejected Reason=NativeClassUnavailable";
        return false;
    }
    AlphaRefs next;
    if(!next.local.bind(local,"Instance",K::Reference,true,error,local) ||
       !next.tables[0].bind(main,local,"liquidAlpha",error) ||
       !next.tables[1].bind(main,local,"activeLiquidAlpha",error))return false;
    next.ready=true;alphaRefs=next;return true;
}
}

bool ensureWaterAlphaTables(int total,std::string* error) {
    if(total<kVanillaWaterStyleCount || total>kMaxWaterStyleCount || gameThreadId()<=0 || gameThreadId()!=static_cast<int>(gettid())) {
        if(error)*error="[Native][ERROR][WaterStyle] Stage=EnsureAlphaTables Expected=ObservedUnityThreadAndCount15.."+
            std::to_string(kMaxWaterStyleCount)+" Actual=Count:"+std::to_string(total)+",UnityThread:"+
            std::to_string(gameThreadId())+",CallerThread:"+std::to_string(gettid())+" Result=Rejected Reason=ThreadOrCountMismatch";
        return false;
    }
    if(!bindAlphaTables(error))return false;
    Il2CppObject* local=nullptr;
    if(!alphaRefs.local.read(nullptr,&local,error))return false;
    struct Snapshot {Il2CppObject* local;} snapshot{local};
    const auto current=[](void* data) {
        auto* snapshot=static_cast<Snapshot*>(data);Il2CppObject* now=nullptr;
        return alphaRefs.local.read(nullptr,&now,nullptr) && now==snapshot->local;
    };
    std::array<il2cpp::PrimitiveArrayTarget,2> targets;
    for(size_t i=0;i<targets.size();++i)targets[i]={&alphaRefs.tables[i].field,alphaRefs.tables[i].local?local:nullptr};
    return il2cpp::growPrimitiveArrays(targets,static_cast<size_t>(total),error,current,&snapshot,"WaterStyle");
}
}
