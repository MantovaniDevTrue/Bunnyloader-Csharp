#include "experiments/csharp/ManagedUndergroundBackgrounds.h"
#include "content/backgrounds/UndergroundBackgrounds.h"
#include <mutex>
#include <atomic>
#include <cstring>

namespace bl::csharp {
namespace {
std::mutex mutex;
std::mutex transaction;
std::atomic<void(*)()> dispatcher{nullptr};
uint64_t revision=0;
int requested=-1;
bool live=false,done=false;
runtime::underground::Slots proposed{};
thread_local std::string lastError;
bool fill(int slot,runtime::underground::Slots* values,std::string* error) {
    std::unique_lock flight(transaction,std::try_to_lock);
    if(!flight){*error="[CSharp][WARN][UndergroundBackground] Stage=FillTextureArray Result=Fallback Reason=DispatcherBusy ContentId="+std::to_string(slot);return false;}
    auto callback=dispatcher.load(std::memory_order_acquire);
    if(!callback){*error="[CSharp][ERROR][UndergroundBackground] Stage=FillTextureArray Result=Rejected Reason=ManagedDispatcherUnavailable";return false;}
    uint64_t session;
    {std::lock_guard lock(mutex);session=++revision;requested=slot;live=true;done=false;}
    callback();
    std::lock_guard lock(mutex);
    if(revision!=session || !live || !done) {
        live=false;*error="[CSharp][WARN][UndergroundBackground] Stage=FillTextureArray Result=Fallback Reason=CallbackFailedOrDeadlineExceeded ContentId="+std::to_string(slot);return false;
    }
    *values=proposed;live=false;return true;
}
}
void managedUndergroundDispatch(void(*dispatch)()) {
    dispatcher.store(dispatch,std::memory_order_release);
    runtime::underground::provider(runtime::underground::Language::Managed,nullptr,fill);
}
void removeManagedUndergroundDispatch() {
    dispatcher.store(nullptr,std::memory_order_release);
    std::lock_guard lock(mutex);++revision;live=false;
}
extern "C" __attribute__((visibility("default"))) int bunny_underground_reserve(const char* mod,const char* name) {
    lastError.clear();
    if(!mod || !name || strnlen(mod,129)>128 || strnlen(name,241)>240){lastError="[CSharp][ERROR][UndergroundBackground] Stage=Reserve Reason=InvalidIdentity Result=Rejected";return -1;}
    return runtime::underground::reserve(mod,name,runtime::underground::Language::Managed,&lastError);
}
extern "C" __attribute__((visibility("default"))) int bunny_underground_find(const char* mod,const char* name) {
    lastError.clear();
    if(!mod || !name || strnlen(mod,129)>128 || strnlen(name,241)>240){lastError="[CSharp][ERROR][UndergroundBackground] Stage=ResolveStyle Reason=InvalidIdentity Result=Rejected";return -1;}
    const int slot=runtime::underground::find(mod,name);
    if(slot<0)lastError="[CSharp][ERROR][UndergroundBackground] Mod="+std::string(mod)+" Class="+name+" Stage=ResolveStyle Expected=RegisteredStyle Actual=Missing Result=Rejected Reason=StyleNotRegistered";
    return slot;
}
extern "C" __attribute__((visibility("default"))) int bunny_underground_select(int slot,int priority,double weight,int active) {
    lastError.clear();
    if(active!=0 && active!=1){lastError="[CSharp][ERROR][UndergroundBackground] Stage=Select Expected=Boolean Actual="+std::to_string(active)+" Result=Rejected Reason=InvalidActivation";return 0;}
    return runtime::underground::select(slot,priority,weight,active!=0,&lastError)?1:0;
}
extern "C" __attribute__((visibility("default"))) void bunny_underground_retire(int slot){runtime::underground::retire(slot);}
extern "C" __attribute__((visibility("default"))) const char* bunny_underground_last_error(){return lastError.c_str();}
extern "C" __attribute__((visibility("default"))) int bunny_underground_fill_read(int64_t* session,int* slot) {
    if(!session || !slot)return 0;
    std::lock_guard lock(mutex);if(!live)return 0;
    *session=int64_t(revision);*slot=requested;return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_underground_fill_write(int64_t session,int slot,int a,int b,int c,int d,int e) {
    std::lock_guard lock(mutex);if(!live || done || revision!=uint64_t(session) || requested!=slot)return 0;
    proposed={a,b,c,d,e};done=true;return 1;
}
}
