#include "experiments/csharp/ManagedSurfaceBackgrounds.h"
#include "content/backgrounds/SurfaceBackgrounds.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace bl::csharp {
static_assert(sizeof(int)==4 && sizeof(float)==4 && sizeof(double)==8 && sizeof(int64_t)==8);
namespace {
namespace surface=runtime::surface;
std::mutex surfaceMutex,surfaceTransaction;
std::atomic<void(*)()> surfaceDispatcher{nullptr};
uint64_t surfaceRevision=0;
bool surfaceLive=false,surfaceDone=false,surfaceFadesWritten=false;
bool surfaceRejected=false;
std::string surfaceRejection;
surface::Request surfaceRequest;
surface::Reply surfaceReply;
thread_local std::string surfaceError;
bool validSession(int64_t session,int slot) {
    return surfaceLive && !surfaceDone && !surfaceRejected && surfaceRevision==uint64_t(session) && surfaceRequest.slot==slot;
}
int surfaceReject(const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    surfaceError="[CSharp][ERROR][NativeBridge] API=BunnySurfaceBackgroundStyle Stage="+std::string(stage)+" Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;return 0;
}
bool surfaceInvoke(const surface::Request& request,surface::Reply* reply,std::string* error) {
    std::unique_lock flight(surfaceTransaction,std::try_to_lock);
    if(!flight){*error="[CSharp][WARN][SurfaceBackground] Stage=Invoke Result=Fallback Reason=DispatcherBusy ContentId="+std::to_string(request.slot);return false;}
    const auto callback=surfaceDispatcher.load(std::memory_order_acquire);
    if(!callback){*error="[CSharp][ERROR][SurfaceBackground] Stage=Invoke Result=Rejected Reason=ManagedDispatcherUnavailable";return false;}
    uint64_t session;
    {std::lock_guard lock(surfaceMutex);session=++surfaceRevision;surfaceRequest=request;surfaceReply={};surfaceLive=true;surfaceDone=false;surfaceFadesWritten=false;surfaceRejected=false;surfaceRejection.clear();}
    callback();
    std::lock_guard lock(surfaceMutex);
    if(surfaceRevision==session && surfaceLive && surfaceRejected){*error=surfaceRejection;surfaceLive=false;return false;}
    if(surfaceRevision!=session || !surfaceLive || !surfaceDone) {
        const char* reason=surfaceRevision!=session?"CallbackSessionInvalidated":!surfaceLive?"CallbackCancelled":"CallbackReplyMissing";
        surfaceLive=false;*error="[CSharp][WARN][SurfaceBackground] Stage=Invoke Expected=CompletedCurrentRevision Actual=NoValidReply Result=Fallback Reason="+
            std::string(reason)+" ContentId="+std::to_string(request.slot);return false;
    }
    *reply=std::move(surfaceReply);surfaceLive=false;return true;
}
bool surfaceIdentity(const char* mod,const char* name) {
    if(mod && name && strnlen(mod,129)<=128 && strnlen(name,241)<=240)return true;
    surfaceError="[CSharp][ERROR][SurfaceBackground] Stage=ResolveIdentity Expected=BoundedModUidAndClass Actual=NullOrTooLong Result=Rejected Reason=InvalidIdentity";return false;
}
uint32_t pack(surface::Color color){return uint32_t(color.r)|(uint32_t(color.g)<<8)|(uint32_t(color.b)<<16)|(uint32_t(color.a)<<24);}
}
void managedSurfaceDispatch(void(*dispatch)()) {
    surfaceDispatcher.store(dispatch,std::memory_order_release);
    surface::provider(surface::Language::Managed,nullptr,nullptr,surfaceInvoke);
}
void removeManagedSurfaceDispatch() {
    surfaceDispatcher.store(nullptr,std::memory_order_release);
    std::lock_guard lock(surfaceMutex);++surfaceRevision;surfaceLive=false;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_reserve(const char* mod,const char* name) {
    surfaceError.clear();if(!surfaceIdentity(mod,name))return -1;
    return surface::reserve(mod,name,surface::Language::Managed,&surfaceError);
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_find(const char* mod,const char* name) {
    surfaceError.clear();if(!surfaceIdentity(mod,name))return -1;
    int slot=surface::find(mod,name);
    if(slot<0)surfaceError="[CSharp][ERROR][SurfaceBackground] Mod="+std::string(mod)+" Class="+name+" Stage=ResolveStyle Expected=RegisteredStyle Actual=Missing Result=Rejected Reason=StyleNotRegistered";
    return slot;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_vanilla_count() {
    surfaceError.clear();return surface::vanillaCount(&surfaceError);
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_select(int slot,int priority,double weight,int active,int menu) {
    surfaceError.clear();
    if((active!=0 && active!=1) || (menu!=0 && menu!=1)) {
        surfaceError="[CSharp][ERROR][SurfaceBackground] Stage=Select Expected=BooleanFlags Actual=OutOfRange Result=Rejected Reason=InvalidActivation";return 0;
    }
    return surface::select(slot,priority,weight,active!=0,menu!=0,&surfaceError)?1:0;
}
extern "C" __attribute__((visibility("default"))) void bunny_surface_retire(int slot){surface::retire(slot);}
extern "C" __attribute__((visibility("default"))) const char* bunny_surface_last_error(){return surfaceError.c_str();}

// ABI v1: fixed-width primitives only, never IL2CPP or managed object pointers.
// The revision guards every read/write; a late worker cannot complete a newer draw.
extern "C" __attribute__((visibility("default"))) int bunny_surface_read(int version,int64_t* session,int* slot,int* operation,double* speed,int* fades) {
    if(version!=1 || !session || !slot || !operation || !speed || !fades)return 0;
    std::lock_guard lock(surfaceMutex);if(!surfaceLive || surfaceDone || surfaceRejected)return 0;
    *session=int64_t(surfaceRevision);*slot=surfaceRequest.slot;*operation=int(surfaceRequest.operation);*speed=surfaceRequest.speed;*fades=int(surfaceRequest.fades.size());return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_context(int64_t session,int slot,double* values,int length,uint32_t* rgba) {
    if(!values || length!=13 || !rgba)return 0;
    std::lock_guard lock(surfaceMutex);if(!validSession(session,slot))return 0;
    const auto& r=surfaceRequest;
    const double data[]={r.alpha,r.close.screenX,r.close.screenY,r.close.worldSurface,double(r.close.screenWidth),r.close.stepTop,r.close.scAdj,double(r.close.push),double(r.back.loops),r.back.startX,r.back.topY,r.back.step,r.back.scale};
    std::memcpy(values,data,sizeof(data));*rgba=pack(r.color);return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_fades(int64_t session,int slot,float* values,int length,int write) {
    if(!values || length<0 || length>4096 || (write!=0 && write!=1))return 0;
    std::lock_guard lock(surfaceMutex);
    if(!validSession(session,slot) || surfaceRequest.operation!=surface::Operation::Fades || size_t(length)!=surfaceRequest.fades.size())return 0;
    if(write) {
        for(int i=0;i<length;++i)if(!std::isfinite(values[i]))return 0;
        surfaceReply.fades.assign(values,values+length);surfaceFadesWritten=true;
    }else std::memcpy(values,surfaceRequest.fades.data(),size_t(length)*sizeof(float));
    return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_draw(int64_t session,int slot,int texture,const double* values,int length,uint32_t rgba) {
    surfaceError.clear();
    if(!values || length!=13 || texture<0)return surfaceReject("QueueDraw","InvalidCommandEnvelope","NonNull13DoublesAndNonNegativeTexture","Length:"+std::to_string(length)+",Texture:"+std::to_string(texture));
    for(int i=0;i<length;++i)if(!std::isfinite(values[i]))return surfaceReject("QueueDraw","NonFiniteCommandValue","FiniteDoubleAtIndex:"+std::to_string(i),std::to_string(values[i]));
    // x/y, source x/y/w/h, hasSource, rotation, origin x/y, scale, effects, depth.
    for(int i=2;i<=6;++i)if(values[i]<0 || values[i]>32768 || std::floor(values[i])!=values[i])return surfaceReject("QueueDraw","InvalidSourceRectangleValue","Integer:0..32768AtIndex:"+std::to_string(i),std::to_string(values[i]));
    if(values[6]>1 || values[10]<=0 || values[10]>32768)return surfaceReject("QueueDraw","InvalidSourceFlagOrScale","SourceFlag:0..1,Scale:0..32768ExclusiveZero","Flag:"+std::to_string(values[6])+",Scale:"+std::to_string(values[10]));
    const double effects=values[11],depth=values[12];
    if(effects<0 || effects>3 || std::floor(effects)!=effects || depth<0 || depth>1)return surfaceReject("QueueDraw","InvalidEffectsOrDepth","Effects:Int32:0..3,Depth:0..1","Effects:"+std::to_string(effects)+",Depth:"+std::to_string(depth));
    std::lock_guard lock(surfaceMutex);
    if(!validSession(session,slot))return surfaceReject("QueueDraw","CallbackRevisionExpired","CurrentLiveRevisionAndSlot",std::to_string(session)+","+std::to_string(slot));
    if(surfaceRequest.operation!=surface::Operation::Close)return surfaceReject("QueueDraw","DrawOutsideCloseCallback","CloseOperation",std::to_string(int(surfaceRequest.operation)));
    if(surfaceReply.custom.size()>=128)return surfaceReject("QueueDraw","DrawCommandCapacityExceeded","Below128Commands",std::to_string(surfaceReply.custom.size()));
    surface::CustomDraw command;command.texture=texture;command.color={uint8_t(rgba),uint8_t(rgba>>8),uint8_t(rgba>>16),uint8_t(rgba>>24)};
    auto& p=command.plan;p.loops=1;p.startX=values[0];p.topY=values[1];p.sourceX=int(values[2]);p.sourceY=int(values[3]);p.width=int(values[4]);p.height=int(values[5]);p.hasSource=values[6]!=0;
    p.rotation=float(values[7]);p.originX=float(values[8]);p.originY=float(values[9]);p.scale=float(values[10]);p.effects=int(effects);p.layerDepth=float(depth);
    surfaceReply.custom.push_back(command);return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_complete(int64_t session,int slot,int texture,int defaultClose,double scale,double parallax,double a,double b) {
    if(texture< -1 || (defaultClose!=0 && defaultClose!=1) || !std::isfinite(scale) || !std::isfinite(parallax) || !std::isfinite(a) || !std::isfinite(b))return 0;
    std::lock_guard lock(surfaceMutex);if(!validSession(session,slot))return 0;
    if(surfaceRequest.operation==surface::Operation::Fades && !surfaceFadesWritten)return 0;
    surfaceReply.texture=texture;surfaceReply.defaultClose=defaultClose!=0;surfaceReply.close={scale,parallax,a,b};surfaceDone=true;return 1;
}
extern "C" __attribute__((visibility("default"))) int bunny_surface_reject(int64_t session,int slot,const char* diagnostic) {
    if(!diagnostic)return 0;
    const size_t size=strnlen(diagnostic,16385);if(!size)return 0;
    std::lock_guard lock(surfaceMutex);if(!validSession(session,slot))return 0;
    surfaceRejection.assign(diagnostic,std::min(size,size_t(16384)));
    if(size>16384)surfaceRejection+=" DiagnosticTruncated=1";
    surfaceRejected=true;return 1;
}
}
