#include "content/backgrounds/SurfaceTables.h"

namespace bl::runtime::surface {
bool growTables(const std::array<TableTarget,4>& targets,size_t total,std::string* error,bool(*isCurrent)(void*),void* context) {
    auto& a=il2cpp::api();
    auto fail=[&](const char* reason,size_t index,const std::string& actual){
        if(error)*error="[Native][ERROR][SurfaceBackground] Stage=GrowTables Field="+
            (index<targets.size() && targets[index].field?targets[index].field->contract().label:"Unknown")+
            " Expected=FourCompatibleRootedTables Total="+std::to_string(total)+" Actual="+actual+" Result=Rejected Reason="+reason;
        return false;
    };
    if(total==0 || total>4096)return fail("InvalidCapacity",4,"Total:"+std::to_string(total));
    if(!a.array_new || !a.gchandle_new || !a.gchandle_free || !a.gchandle_get_target)
        return fail("MetadataUnavailable",4,std::string("Missing=")+(!a.array_new?"array_new ":"")+(!a.gchandle_new?"gchandle_new ":"")+
                    (!a.gchandle_free?"gchandle_free ":"")+(!a.gchandle_get_target?"gchandle_get_target":""));
    struct Row {Il2CppArray* original=nullptr;uint32_t owner=0,before=0,after=0;size_t bytes=0;};
    std::array<Row,4> rows{};
    struct Cleanup {
        std::array<Row,4>& rows;
        ~Cleanup(){for(auto& r:rows)for(auto id:{r.owner,r.before,r.after})if(id)il2cpp::api().gchandle_free(id);}
    } cleanup{rows};
    using K=il2cpp::CheckedProperty::Kind;
    bool resize=false;
    for(size_t i=0;i<targets.size();++i) {
        const auto& t=targets[i];auto& r=rows[i];
        if(!t.field)return fail("MissingTableContract",i,"Null");
        const auto& p=t.field->contract();
        if(p.kind!=K::SingleArray && p.kind!=K::BooleanArray)
            return fail("NativeTableElementMismatch",i,il2cpp::CheckedProperty::expectedName(p.kind));
        for(size_t k=0;k<i;++k)if(targets[k].field->contract().field==p.field && targets[k].owner==t.owner)
            return fail("DuplicateTableTarget",i,"SameFieldAndOwner");
        r.bytes=p.kind==K::BooleanArray?1:4;
        if(!t.field->validate(t.owner,&r.original,sizeof(r.original),error) ||
           !p.read(t.owner,&r.original,error) || !p.array(r.original,0,error))return false;
        resize|=r.original->length<total;
    }
    // Ensure runs every draw because the local-user object can be recreated.
    // The usual already-sized path needs neither allocations nor GC handles.
    if(!resize) {
        if(isCurrent && !isCurrent(context))return fail("WorldOrOwnerGenerationChanged",4,"StaleSnapshot");
        return true;
    }
    for(size_t i=0;i<targets.size();++i) {
        const auto& t=targets[i];auto& r=rows[i];const auto& p=t.field->contract();
        if(!p.isStatic) {
            r.owner=a.gchandle_new(static_cast<Il2CppObject*>(t.owner),false);
            if(!r.owner)return fail("GcRootAllocationFailed",i,"OwnerRoot");
        }
        r.before=a.gchandle_new(reinterpret_cast<Il2CppObject*>(r.original),false);
        if(!r.before)return fail("GcRootAllocationFailed",i,"OriginalArrayRoot");
    }
    for(size_t i=0;i<targets.size();++i) {
        auto& r=rows[i];auto* original=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.before));
        if(original->length>=total)continue;
        const auto& p=targets[i].field->contract();
        auto* next=a.array_new(a.class_get_element_class(p.declared),total);
        if(!next)return fail("NativeArrayAllocationFailed",i,"NullArray");
        r.after=a.gchandle_new(reinterpret_cast<Il2CppObject*>(next),false);
        if(!r.after)return fail("GcRootAllocationFailed",i,"ReplacementArrayRoot");
        next=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.after));
        original=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.before));
        if(!p.array(next,total,error) || next->length!=total)return fail("NativeArrayAllocationMismatch",i,"ReplacementLengthOrType");
        std::memcpy(arrayData(next),arrayData(original),original->length*r.bytes);
    }
    if(isCurrent && !isCurrent(context))return fail("WorldOrOwnerGenerationChanged",4,"StaleSnapshot");
    for(size_t i=0;i<targets.size();++i) {
        auto& r=rows[i];const auto& t=targets[i];
        void* owner=r.owner?a.gchandle_get_target(r.owner):nullptr;
        Il2CppArray* current=nullptr;
        auto* original=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.before));
        auto* next=r.after?reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.after)):original;
        if(!t.field->contract().read(owner,&current,error) || !t.field->validate(owner,&next,sizeof(next),error))return false;
        if(current!=original)return fail("TableChangedDuringAllocation",i,"OriginalArrayReplaced");
    }
    for(size_t i=0;i<targets.size();++i)if(rows[i].after) {
        auto& r=rows[i];auto* next=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(r.after));
        // No managed callbacks or allocations after preflight. These direct
        // field writes cannot throw; all four rows are already valid.
        if(!targets[i].field->write(r.owner?a.gchandle_get_target(r.owner):nullptr,&next,sizeof(next),error)) {
            const std::string originalError=error?*error:"";std::string rollbackError;size_t restored=0;
            for(size_t k=0;k<i;++k)if(rows[k].after) {
                auto* before=reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(rows[k].before));
                if(targets[k].field->write(rows[k].owner?a.gchandle_get_target(rows[k].owner):nullptr,&before,sizeof(before),&rollbackError))++restored;
            }
            if(error)*error=originalError+" Consequence=PublicationRejected RestoredTables="+std::to_string(restored)+
                (rollbackError.empty()?"":" RollbackFailure="+rollbackError);
            return false;
        }
    }
    return true;
}
}
