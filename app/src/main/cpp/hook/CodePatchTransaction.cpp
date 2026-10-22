#include "hook/CodePatchTransaction.h"
#include "il2cpp/Api.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace bl::runtime {
namespace {
constexpr uintptr_t maxMethodBytes=512*1024;
struct Page {uintptr_t start;size_t size;int protection;};
struct Edit {uint32_t* address;uint32_t before,after;};
std::mutex patchMutex;
std::vector<Page> pendingProtection;

std::string failure(const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    return "[Native][ERROR][CodePatch] Stage="+std::string(stage)+" Expected="+expected+
        " Actual="+actual+" Reason="+reason;
}
bool mapping(uintptr_t address,uintptr_t* end,int* protection) {
    FILE* input=std::fopen("/proc/self/maps","r");
    if(!input)return false;
    char line[1024];bool found=false;
    while(std::fgets(line,sizeof(line),input)) {
        unsigned long first=0,last=0;char modes[5]{};
        if(std::sscanf(line,"%lx-%lx %4s",&first,&last,modes)!=3 || address<first || address>=last)continue;
        *end=last;*protection=(modes[0]=='r'?PROT_READ:0)|(modes[1]=='w'?PROT_WRITE:0)|(modes[2]=='x'?PROT_EXEC:0);
        found=true;break;
    }
    std::fclose(input);return found;
}
bool restore(const std::vector<Page>& pages,std::string* error) {
    pendingProtection.clear();int firstError=0;
    for(const auto& page:pages) {
        if(mprotect(reinterpret_cast<void*>(page.start),page.size,page.protection)==0)continue;
        if(!firstError)firstError=errno;
        pendingProtection.push_back(page);
    }
    if(pendingProtection.empty())return true;
    if(error)*error=failure("RestoreProtection","PageProtectionRejected","OriginalProtectionForAllPages",
        "PendingPages:"+std::to_string(pendingProtection.size())+",Errno:"+std::to_string(firstError)+":"+std::strerror(firstError));
    return false;
}
bool repair(std::string* error) {
    if(pendingProtection.empty())return true;
    // restore replaces the pending list, so retain a distinct input vector.
    const auto pages=pendingProtection;return restore(pages,error);
}
bool branch(uint32_t value,bool exitOnEqual) {
    if((value&0xFF000010u)!=0x54000000u)return false;
    const auto condition=value&15u;
    return exitOnEqual?condition==0:condition==1 || condition==0xB || condition==3;
}
}

bool repairCodePatchProtection(std::string* error) {
    std::lock_guard lock(patchMutex);return repair(error);
}

CodePatchResult patchLoopLimits(const MethodInfo* method,std::span<const LoopLimitPatch> patches) {
    std::lock_guard lock(patchMutex);CodePatchResult result;
    auto reject=[&](const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
        result.detail=failure(stage,reason,expected,actual);return result;
    };
    if(!repair(&result.detail)){result.permissionsRestored=false;return result;}
    if(!method || patches.empty() || patches.size()>16)
        return reject("Preflight","InvalidRequest","MethodAnd1..16Changes","Changes:"+std::to_string(patches.size()));
    auto& api=il2cpp::api();
    if(!api.method_get_class || !api.class_get_methods)
        return reject("Preflight","MetadataUnavailable","method_get_class,class_get_methods",
            std::string("MissingExports:")+(!api.method_get_class?"method_get_class ":"")+(!api.class_get_methods?"class_get_methods":""));
    const auto pointer=[](const MethodInfo* value){return *reinterpret_cast<void* const*>(value);};
    const auto start=reinterpret_cast<uintptr_t>(pointer(method));
    if(!start || start%alignof(uint32_t) || start>std::numeric_limits<uintptr_t>::max()-maxMethodBytes)
        return reject("Preflight","InvalidCodeAddress","NonNullAlignedCode","NullMisalignedOrOverflow");
    auto* owner=api.method_get_class(method);
    if(!owner)return reject("Preflight","MetadataUnavailable","DeclaringClass","Missing");
    uintptr_t end=start+maxMethodBytes;bool bounded=false;void* iterator=nullptr;
    while(const auto* other=api.class_get_methods(owner,&iterator)) {
        const auto address=reinterpret_cast<uintptr_t>(pointer(other));
        if(address>start && address<=end){end=address;bounded=true;}
    }
    // Do not scan a guessed 512 KiB for a final/stripped method.
    if(!bounded || end%alignof(uint32_t) || end-start<8)
        return reject("Preflight","MethodBoundaryUnavailable","FollowingAlignedMetadataMethodWithin512KiB","MissingOrInvalidBoundary");
    const long pageSize=sysconf(_SC_PAGESIZE);
    if(pageSize<=0 || (pageSize&(pageSize-1)))
        return reject("Preflight","InvalidPageSize","PositivePowerOfTwo",std::to_string(pageSize));
    // Validate every page before touching the first instruction. A following
    // metadata pointer alone does not prove that the intervening span is mapped.
    for(uintptr_t at=start;at<end;) {
        uintptr_t mapEnd=0;int protection=0;
        if(!mapping(at,&mapEnd,&protection) || (protection&(PROT_READ|PROT_EXEC))!=(PROT_READ|PROT_EXEC))
            return reject("Preflight","CodeMappingUnavailable","ReadableExecutableMethodSpan","UnmappedOrNotRX");
        at=std::min(end,mapEnd);
    }
    std::vector<Edit> edits;edits.reserve(patches.size());
    for(size_t i=0;i<patches.size();++i) {
        const auto change=patches[i];
        if(change.before>4095 || change.after>4095 || change.before==change.after)
            return reject("Preflight","ImmediateOutOfRangeOrUnchanged","Distinct12BitLimits","Change:"+std::to_string(i)+",Before:"+std::to_string(change.before)+",After:"+std::to_string(change.after));
        uint32_t* match=nullptr;size_t count=0;
        for(auto* at=reinterpret_cast<uint32_t*>(start);reinterpret_cast<uintptr_t>(at)+8<=end;++at) {
            if((*at&0xFFC0001Fu)!=0xF100001Fu || ((*at>>10)&4095u)!=change.before || !branch(at[1],change.exitOnEqual))continue;
            match=at;++count;
        }
        if(count!=1)return reject("Preflight","PatternCountMismatch","ExactlyOneLoopComparisonPerChange",
            "Change:"+std::to_string(i)+",Immediate:"+std::to_string(change.before)+",Matches:"+std::to_string(count));
        if(std::any_of(edits.begin(),edits.end(),[&](const Edit& edit){return edit.address==match;}))
            return reject("Preflight","OverlappingChanges","DistinctInstructionPerChange","Change:"+std::to_string(i));
        edits.push_back({match,*match,(*match&~(4095u<<10))|(change.after<<10)});
    }
    std::vector<Page> pages;
    for(const auto& edit:edits) {
        const uintptr_t address=reinterpret_cast<uintptr_t>(edit.address)&~static_cast<uintptr_t>(pageSize-1);
        if(std::any_of(pages.begin(),pages.end(),[&](const Page& page){return page.start==address;}))continue;
        uintptr_t mapEnd=0;int protection=0;
        if(!mapping(address,&mapEnd,&protection) || mapEnd-address<static_cast<uintptr_t>(pageSize))
            return reject("Preflight","CodeMappingChanged","WholeMappedCodePage","MissingOrPartialPage");
        pages.push_back({address,static_cast<size_t>(pageSize),protection});
    }
    std::vector<Page> writable;writable.reserve(pages.size());
    for(const auto& page:pages) {
        if(mprotect(reinterpret_cast<void*>(page.start),page.size,page.protection|PROT_WRITE)!=0) {
            const int saved=errno;
            result.permissionsRestored=restore(writable,&result.detail);
            const auto protectionError=result.detail;
            result.detail=failure("AcquireWrite","PageProtectionRejected","AllPagesWritableBeforeFirstEdit",
                "Acquired:"+std::to_string(writable.size())+",Errno:"+std::to_string(saved)+":"+std::strerror(saved));
            if(!protectionError.empty())result.detail+=" Restoration="+protectionError;
            return result;
        }
        writable.push_back(page);
    }
    for(const auto& edit:edits)if(*edit.address!=edit.before) {
        result.detail=failure("Commit","CodeChangedDuringPreflight","UnchangedVerifiedInstructions","InstructionConflict");
        std::string restoreError;result.permissionsRestored=restore(writable,&restoreError);
        if(!restoreError.empty())result.detail+=" Restoration="+restoreError;
        return result;
    }
    // No fallible calls between edits. This is a group publication on the
    // engine thread, not a claim of atomic multi-word execution across threads.
    for(const auto& edit:edits)*edit.address=edit.after;
    for(const auto& edit:edits)__builtin___clear_cache(reinterpret_cast<char*>(edit.address),reinterpret_cast<char*>(edit.address+1));
    result.applied=true;result.permissionsRestored=restore(writable,&result.detail);return result;
}
}
