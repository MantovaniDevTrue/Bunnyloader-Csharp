#include "il2cpp/Api.h"
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <array>
#include <set>
struct Il2CppClass;
struct MethodInfo {void* pointer;Il2CppClass* owner;};
struct Il2CppClass {std::vector<MethodInfo*> methods;};
namespace {
bl::il2cpp::Api exports;
int protectCalls=0;std::set<int> rejectCalls;
uint32_t* mutateOnAcquire=nullptr;
int protect(void* at,size_t bytes,int mode) {
    ++protectCalls;
    if(rejectCalls.contains(protectCalls)){errno=EACCES;return -1;}
    if(mutateOnAcquire && protectCalls==2)*mutateOnAcquire=0xd503201fu;
    return ::mprotect(at,bytes,mode);
}
}
// Inject only the OS failure boundary. Instruction discovery, metadata bounds,
// transactions, mapping checks and permission recovery are production code.
#define mprotect protect
#include "hook/CodePatchTransaction.cpp"
#undef mprotect
namespace bl::il2cpp {Api& api(){return exports;}}
namespace {
int checks=0;
#define CHECK(value) do{++checks;if(!(value)){std::cerr<<"FAIL "<<__LINE__<<" "<<#value<<'\n';std::abort();}}while(false)
constexpr uint32_t nop=0xd503201fu;
uint32_t cmp(unsigned value){return 0xf100001fu|(value<<10);}
const std::array<bl::runtime::LoopLimitPatch,2> changes={bl::runtime::LoopLimitPatch{23,24,false},{15,16,true}};
}
int main() {
    using namespace bl::runtime;
    exports.method_get_class=[](const MethodInfo* method){return method->owner;};
    exports.class_get_methods=[](Il2CppClass* cls,void** iterator)->const MethodInfo* {
        size_t index=reinterpret_cast<uintptr_t>(*iterator);
        if(index>=cls->methods.size())return nullptr;
        *iterator=reinterpret_cast<void*>(index+1);return cls->methods[index];
    };
    const size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto* memory=static_cast<uint32_t*>(::mmap(nullptr,page*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    CHECK(memory!=MAP_FAILED);
    auto* second=reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(memory)+page);
    Il2CppClass owner;
    MethodInfo method{memory,&owner},next{second+8,&owner};owner.methods={&method,&next};
    auto prepare=[&] {
        CHECK(repairCodePatchProtection(nullptr));rejectCalls.clear();protectCalls=0;mutateOnAcquire=nullptr;
        CHECK(::mprotect(memory,page*3,PROT_READ|PROT_WRITE)==0);
        std::fill(memory,memory+page*3/4,nop);
        memory[0]=cmp(23);memory[1]=0x54000001u;second[0]=cmp(15);second[1]=0x54000000u;
        CHECK(::mprotect(memory,page*3,PROT_READ|PROT_EXEC)==0);
        owner.methods={&method,&next};method.pointer=memory;next.pointer=second+8;
    };
    prepare();auto result=patchLoopLimits(&method,changes);
    CHECK(result.applied && result.permissionsRestored && result.detail.empty());
    CHECK(memory[0]==cmp(24) && second[0]==cmp(16));CHECK(protectCalls==4);
    uintptr_t end=0;int protection=0;CHECK(mapping(reinterpret_cast<uintptr_t>(memory),&end,&protection));
    CHECK(protection==(PROT_READ|PROT_EXEC));
    // The second pattern is absent: the first instruction must stay untouched.
    prepare();CHECK(::mprotect(second,page,PROT_READ|PROT_WRITE)==0);second[0]=nop;
    CHECK(::mprotect(second,page,PROT_READ|PROT_EXEC)==0);
    result=patchLoopLimits(&method,changes);CHECK(!result.applied && protectCalls==0 && memory[0]==cmp(23));
    CHECK(result.detail.find("Change:1,Immediate:15,Matches:0")!=std::string::npos);
    // Multiple matching loops are ambiguous, not permission to patch both.
    prepare();CHECK(::mprotect(second,page,PROT_READ|PROT_WRITE)==0);second[2]=cmp(15);second[3]=0x54000000u;
    CHECK(::mprotect(second,page,PROT_READ|PROT_EXEC)==0);
    result=patchLoopLimits(&method,changes);CHECK(!result.applied && protectCalls==0 && memory[0]==cmp(23));
    CHECK(result.detail.find("Matches:2")!=std::string::npos);
    // The second page refuses write access. First page is restored without any
    // edit; the next valid request can use the original limits normally.
    prepare();rejectCalls={2};result=patchLoopLimits(&method,changes);
    CHECK(!result.applied && result.permissionsRestored && memory[0]==cmp(23) && second[0]==cmp(15));
    CHECK(protectCalls==3 && result.detail.find("AcquireWrite")!=std::string::npos);
    rejectCalls.clear();protectCalls=0;result=patchLoopLimits(&method,changes);CHECK(result.applied && result.permissionsRestored);
    // Permission restoration can fail after the complete code publication. It
    // must be reported as such, retain pending RX repair and never redo edits.
    prepare();rejectCalls={3};result=patchLoopLimits(&method,changes);
    CHECK(result.applied && !result.permissionsRestored && memory[0]==cmp(24) && second[0]==cmp(16));
    CHECK(pendingProtection.size()==1 && result.detail.find("RestoreProtection")!=std::string::npos);
    rejectCalls={5};std::string error;CHECK(!repairCodePatchProtection(&error));CHECK(pendingProtection.size()==1);
    rejectCalls.clear();CHECK(repairCodePatchProtection(&error));CHECK(pendingProtection.empty());
    CHECK(memory[0]==cmp(24) && second[0]==cmp(16));
    // Acquisition rollback itself may hit an OS failure too. Refuse further
    // edits until the outstanding protection is genuinely repaired.
    prepare();rejectCalls={2,3};result=patchLoopLimits(&method,changes);
    CHECK(!result.applied && !result.permissionsRestored && memory[0]==cmp(23));
    rejectCalls={4};result=patchLoopLimits(&method,changes);CHECK(!result.applied && !result.permissionsRestored);
    rejectCalls.clear();CHECK(repairCodePatchProtection(nullptr));CHECK(memory[0]==cmp(23));
    // Concurrent modification before commit is detected after all permissions
    // are acquired; the other verified comparison is never written.
    prepare();mutateOnAcquire=memory;result=patchLoopLimits(&method,changes);
    CHECK(!result.applied && result.permissionsRestored && second[0]==cmp(15));
    CHECK(result.detail.find("InstructionConflict")!=std::string::npos);mutateOnAcquire=nullptr;
    prepare();owner.methods={&method};result=patchLoopLimits(&method,changes);
    CHECK(!result.applied && protectCalls==0 && result.detail.find("MethodBoundaryUnavailable")!=std::string::npos);
    prepare();CHECK(::mprotect(second,page,PROT_NONE)==0);result=patchLoopLimits(&method,changes);
    CHECK(!result.applied && protectCalls==0 && result.detail.find("CodeMappingUnavailable")!=std::string::npos);
    CHECK(::mprotect(second,page,PROT_READ|PROT_EXEC)==0);
    prepare();const auto enumerate=exports.class_get_methods;exports.class_get_methods=nullptr;
    result=patchLoopLimits(&method,changes);CHECK(!result.applied && result.detail.find("MissingExports:class_get_methods")!=std::string::npos);exports.class_get_methods=enumerate;
    CHECK(!patchLoopLimits(nullptr,changes).applied);CHECK(!patchLoopLimits(&method,{}).applied);
    for(const auto invalid:std::array<LoopLimitPatch,3>{{{23,4096,false},{4096,24,false},{23,23,false}}}) {
        std::array<LoopLimitPatch,1> change={invalid};result=patchLoopLimits(&method,change);
        CHECK(!result.applied && protectCalls==0 && memory[0]==cmp(23));
    }
    std::array<LoopLimitPatch,2> overlap={changes[0],changes[0]};result=patchLoopLimits(&method,overlap);
    CHECK(!result.applied && protectCalls==0 && result.detail.find("OverlappingChanges")!=std::string::npos);
    // All original continuation conditions NE/LT/LO and the upper imm12 edge.
    for(const uint32_t condition:{1u,0xbu,3u}) {
        prepare();CHECK(::mprotect(memory,page,PROT_READ|PROT_WRITE)==0);memory[1]=0x54000000u|condition;
        CHECK(::mprotect(memory,page,PROT_READ|PROT_EXEC)==0);
        std::array<LoopLimitPatch,2> edge={LoopLimitPatch{23,4095,false},{15,4087,true}};
        result=patchLoopLimits(&method,edge);CHECK(result.applied && memory[0]==cmp(4095) && second[0]==cmp(4087));
    }
    CHECK(::munmap(memory,page*3)==0);
    std::cout<<"WATER_CODE_PATCH_TRANSACTION_PASS checks="<<checks<<" quickjs=not-linked apk=not-built\n";
}
