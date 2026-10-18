#include "experiments/csharp/NativeAccess.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include "il2cpp/GuardedCall.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>

namespace bl::csharp {
namespace {
using S=AccessStatus;
using K=AccessKind;
constexpr int MaxScopes=64, MaxBindings=2048, MaxBatch=16, MaxArgs=4;
struct Layout { K kind=K::Void; Il2CppClass* cls=nullptr; size_t bytes=0; std::array<size_t,4> offsets{}; };
struct Scope { std::atomic<uint64_t> token{0}; };
struct Binding {
    uint64_t scope=0;
    int category=0;
    Il2CppClass* cls=nullptr;
    FieldInfo* field=nullptr;
    const MethodInfo* getter=nullptr;
    const MethodInfo* setter=nullptr;
    bool isStatic=false, writable=false;
    size_t offset=0;
    Layout value;
    std::array<Layout,MaxArgs> args{};
    int argc=0;
};
std::array<Scope,MaxScopes> scopes;
std::array<Binding,MaxBindings> bindings;
uint64_t nextScope=1;
std::array<std::atomic<int64_t>,14> stats{};
bool scopeLive(uint64_t token) {
    int slot=static_cast<int>(token&255)-1;
    return slot>=0&&slot<MaxScopes&&token&&scopes[slot].token.load(std::memory_order_acquire)==token;
}
Binding* binding(uint64_t token,int id) {
    return scopeLive(token)&&id>0&&id<=MaxBindings&&bindings[id-1].scope==token?&bindings[id-1]:nullptr;
}
std::string typeName(const Il2CppType* type) {
    auto& a=il2cpp::api();
    if(!type||!a.type_get_name||!a.il2cpp_free)return {};
    char* raw=a.type_get_name(type);
    std::string name=raw?raw:"";
    if(raw)a.il2cpp_free(raw);
    return name;
}
bool layout(const Il2CppType* type,Layout& out) {
    auto& a=il2cpp::api();
    auto name=typeName(type);
    if(name=="System.Void"){out={};return true;}
    if(name.empty()||name.find('&')!=std::string::npos||name.find('*')!=std::string::npos||
       !a.class_from_il2cpp_type||!a.class_is_valuetype||!a.class_value_size)return false;
    Il2CppClass* cls=a.class_from_il2cpp_type(type);
    if(!cls||!a.class_is_valuetype(cls))return false;
    if(a.class_is_enum&&a.class_is_enum(cls)) {
        if(!a.class_enum_basetype||!layout(a.class_enum_basetype(cls),out))return false;
        out.cls=cls;return out.kind==K::Int32||out.kind==K::UInt32||out.kind==K::Int64||out.kind==K::UInt64;
    }
    struct Primitive { const char* name; K kind; size_t bytes; };
    for(const auto& p:{Primitive{"System.Boolean",K::Boolean,1},{"System.Int32",K::Int32,4},
        {"System.UInt32",K::UInt32,4},{"System.Int64",K::Int64,8},{"System.Single",K::Single,4},
        {"System.Double",K::Double,8},{"System.UInt64",K::UInt64,8},
        {"System.Byte",K::UInt32,1},{"System.SByte",K::Int32,1},
        {"System.UInt16",K::UInt32,2},{"System.Int16",K::Int32,2}}) {
        if(name!=p.name)continue;
        uint32_t align=0;
        if(a.class_value_size(cls,&align)!=static_cast<int>(p.bytes))return false;
        out={p.kind,cls,p.bytes,{}};return true;
    }
    bool vector=name=="Microsoft.Xna.Framework.Vector2"||name=="UnityEngine.Vector2";
    bool rectangle=name=="Microsoft.Xna.Framework.Rectangle";
    bool color=name=="Microsoft.Xna.Framework.Color"||name=="Microsoft.Xna.Framework.Graphics.Color";
    if(!vector&&!rectangle&&!color)return false;
    il2cpp::ValueLayout native;
    std::string error;
    if(!il2cpp::resolveValueLayout(cls,&native,&error))return false;
    const int count=vector?2:4;
    const size_t width=color?1:4, expected=static_cast<size_t>(count)*width;
    if(native.size!=expected||!a.field_get_type||!a.field_get_offset||!a.field_get_flags)return false;
    const char* vectorNames[]={"X","Y"};
    const char* unityNames[]={"x","y"};
    const char* rectNames[]={"X","Y","Width","Height"};
    const char* colorNames[]={"R","G","B","A"};
    out={vector?K::Vector2:rectangle?K::Rectangle:K::Color,cls,native.size,{}};
    for(int i=0;i<count;++i) {
        const char* n=vector?(name=="UnityEngine.Vector2"?unityNames[i]:vectorNames[i]):rectangle?rectNames[i]:colorNames[i];
        auto* field=il2cpp::findField(cls,n);
        if(!field||(a.field_get_flags(field)&0x10u)||
           typeName(a.field_get_type(field))!=(color?"System.Byte":vector?"System.Single":"System.Int32")||
           !native.offset(a.field_get_offset(field),width,&out.offsets[i]))return false;
        for(int j=0;j<i;++j)
            if(out.offsets[i]<out.offsets[j]+width&&out.offsets[j]<out.offsets[i]+width)return false;
    }
    return true;
}
AccessValue decode(const Layout& l,const uint8_t* bytes) {
    AccessValue value;value.kind=l.kind;
    if(l.kind==K::Vector2||l.kind==K::Rectangle||l.kind==K::Color) {
        const int count=l.kind==K::Vector2?2:4;
        const size_t width=l.kind==K::Color?1:4;
        uint8_t packed[16]{};
        for(int i=0;i<count;++i)std::memcpy(packed+i*width,bytes+l.offsets[i],width);
        std::memcpy(&value.a,packed,8);std::memcpy(&value.b,packed+8,8);
    } else if(l.bytes) {
        std::memcpy(&value.a,bytes,l.bytes);
        if(l.kind==K::Int32&&l.bytes==1)value.a=static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(value.a)));
        if(l.kind==K::Int32&&l.bytes==2)value.a=static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(value.a)));
    }
    return value;
}
bool encode(const Layout& l,const AccessValue& value,uint8_t* bytes) {
    if(value.kind!=l.kind||value.reserved||l.bytes>16)return false;
    if(l.kind==K::Boolean&&(value.a>1||value.b))return false;
    if(l.kind==K::UInt32&&(value.a>std::numeric_limits<uint32_t>::max()||
       (l.bytes<4&&value.a>=(uint64_t{1}<<(l.bytes*8)))))return false;
    if(l.kind==K::Int32&&l.bytes<4) {
        const auto number=static_cast<int32_t>(value.a);
        const int bits=static_cast<int>(l.bytes*8);
        if(number<-(int64_t{1}<<(bits-1))||number>((int64_t{1}<<(bits-1))-1))return false;
    }
    std::memset(bytes,0,16);
    if(l.kind==K::Vector2||l.kind==K::Rectangle||l.kind==K::Color) {
        const int count=l.kind==K::Vector2?2:4;
        const size_t width=l.kind==K::Color?1:4;
        uint8_t packed[16]{};
        std::memcpy(packed,&value.a,8);std::memcpy(packed+8,&value.b,8);
        for(int i=0;i<count;++i)std::memcpy(bytes+l.offsets[i],packed+i*width,width);
    } else if(l.bytes)std::memcpy(bytes,&value.a,l.bytes);
    return true;
}
const MethodInfo* method(Il2CppClass* cls,const std::string& name,const std::string& parameters,bool isStatic,S& error) {
    auto& a=il2cpp::api();
    if(!a.class_get_methods||!a.method_get_name||!a.method_get_param_count||!a.method_get_param||
       !a.method_get_return_type||!a.method_is_instance||!a.method_is_generic){error=S::Unavailable;return nullptr;}
    const MethodInfo* selected=nullptr;
    for(auto* current=cls;current;current=a.class_get_parent?a.class_get_parent(current):nullptr) {
        void* it=nullptr;
        while(const auto* m=a.class_get_methods(current,&it)) {
            if(name!=a.method_get_name(m)||a.method_is_instance(m)==isStatic||a.method_is_generic(m))continue;
            const unsigned count=a.method_get_param_count(m);
            if(count>MaxArgs)continue;
            std::string signature;
            for(unsigned i=0;i<count;++i){if(i)signature+='|';signature+=typeName(a.method_get_param(m,i));}
            if(signature!=parameters)continue;
            if(selected){error=S::Ambiguous;return nullptr;}
            selected=m;
        }
        if(selected)break;
    }
    error=selected?S::Ok:S::NotFound;return selected;
}
struct Request {
    int op=0,category=0,parent=0,flags=0,member=0,slot=-1,count=0,callCount=0;
    uint64_t scope=0,ticket=0;
    uintptr_t self=0;
    K kind=K::Void;
    bool loadingRead=false;
    std::string name,parameters;
    std::array<int,MaxBatch> ids{};
    std::array<AccessValue,MaxBatch> values{};
    std::array<AccessValue,MaxArgs> arguments{};
};
struct Response { S status=S::Ok; uint64_t number=0; std::array<AccessValue,MaxBatch> values{}; AccessValue methodResult{}; };
struct Rpc {
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<uint64_t> completed{0};
    enum State { Idle,Pending,Done };
    std::atomic<State> state{Idle};
    Request request;
    Response response;
} rpc;

S bind(const Request& r,int& id) {
    if(!scopeLive(r.scope))return S::Expired;
    auto& a=il2cpp::api();
    Binding value;value.scope=r.scope;value.category=r.category;value.isStatic=(r.flags&1)!=0;value.writable=(r.flags&2)!=0;
    if(r.category==0) {
        const auto dot=r.name.rfind('.');
        if(dot==std::string::npos||r.name.rfind("Terraria.",0)!=0)return S::Invalid;
        value.cls=il2cpp::findClassQuiet(r.name.substr(0,dot),r.name.substr(dot+1));
        if(!value.cls)return S::NotFound;
        if(!a.class_is_valuetype||!a.class_is_generic||a.class_is_valuetype(value.cls)||a.class_is_generic(value.cls))return S::Unsupported;
        if(a.runtime_class_init)a.runtime_class_init(value.cls);
    } else {
        auto* parent=binding(r.scope,r.parent);
        if(!parent||parent->category!=0)return S::Expired;
        value.cls=parent->cls;
        if(r.category==1) {
            if(!a.field_get_flags||!a.field_get_type||!a.field_get_offset||!a.class_instance_size)return S::Unavailable;
            value.field=il2cpp::findField(value.cls,r.name);
            if(!value.field)return S::NotFound;
            const auto flags=a.field_get_flags(value.field);
            if(((flags&0x10u)!=0)!=value.isStatic)return S::TypeMismatch;
            if(!layout(a.field_get_type(value.field),value.value)||value.value.kind!=r.kind||r.kind==K::Void)return S::TypeMismatch;
            if(value.writable&&(flags&0x60u))return S::ReadOnly;
            value.offset=a.field_get_offset(value.field);
            if(!value.isStatic) {
                const size_t size=a.class_instance_size(value.cls);
                if(value.offset<sizeof(Il2CppObject)||value.value.bytes>size||value.offset>size-value.value.bytes)return S::TypeMismatch;
            }
        } else if(r.category==2) {
            S error;
            value.getter=method(value.cls,"get_"+r.name,"",value.isStatic,error);
            if(!value.getter)return error;
            if(!layout(a.method_get_return_type(value.getter),value.value)||value.value.kind!=r.kind||r.kind==K::Void)return S::TypeMismatch;
            if(value.writable) {
                value.setter=method(value.cls,"set_"+r.name,typeName(a.method_get_return_type(value.getter)),value.isStatic,error);
                if(!value.setter)return error==S::NotFound?S::ReadOnly:error;
                if(typeName(a.method_get_return_type(value.setter))!="System.Void")return S::TypeMismatch;
            }
        } else if(r.category==3) {
            S error;value.getter=method(value.cls,r.name,r.parameters,value.isStatic,error);
            if(!value.getter)return error;
            if(!layout(a.method_get_return_type(value.getter),value.value)||value.value.kind!=r.kind)return S::TypeMismatch;
            value.argc=static_cast<int>(a.method_get_param_count(value.getter));
            for(int i=0;i<value.argc;++i)
                if(!layout(a.method_get_param(value.getter,i),value.args[i])||value.args[i].kind==K::Void)return S::Unsupported;
        } else return S::Invalid;
    }
    for(int i=0;i<MaxBindings;++i) {
        if(bindings[i].scope&&scopeLive(bindings[i].scope))continue;
        bindings[i]=value;id=i+1;return S::Ok;
    }
    return S::Capacity;
}
S target(const Binding& b,uintptr_t self,void*& out) {
    out=nullptr;if(b.isStatic)return S::Ok;
    auto& a=il2cpp::api();
    if(!self||!a.object_get_class||!a.class_is_assignable_from)return S::Unavailable;
    auto* cls=a.object_get_class(reinterpret_cast<Il2CppObject*>(self));
    if(!cls||!a.class_is_assignable_from(b.cls,cls))return S::TypeMismatch;
    out=reinterpret_cast<void*>(self);return S::Ok;
}
S invoke(const Binding& b,const MethodInfo* m,void* self,void** args,AccessValue& out) {
    auto& a=il2cpp::api();
    if(!a.runtime_invoke)return S::Unavailable;
    Il2CppObject* exception=nullptr;
    auto* boxed=a.runtime_invoke(m,self,args,&exception);
    if(exception)return S::NativeException;
    if(b.value.kind==K::Void){out={};return S::Ok;}
    if(!boxed||!a.object_get_class||a.object_get_class(boxed)!=b.value.cls)return S::TypeMismatch;
    out=decode(b.value,reinterpret_cast<const uint8_t*>(boxed)+sizeof(Il2CppObject));return S::Ok;
}
S read(const Binding& b,uintptr_t self,AccessValue& out) {
    void* object=nullptr;S status=target(b,self,object);if(status!=S::Ok)return status;
    if(b.category==2)return invoke(b,b.getter,object,nullptr,out);
    if(b.category!=1)return S::Invalid;
    alignas(16) uint8_t bytes[16]{};
    if(b.isStatic) {
        if(!il2cpp::api().field_static_get_value)return S::Unavailable;
        il2cpp::api().field_static_get_value(b.field,bytes);
    } else std::memcpy(bytes,reinterpret_cast<const uint8_t*>(object)+b.offset,b.value.bytes);
    out=decode(b.value,bytes);return S::Ok;
}
Response execute(const Request& r) {
    Response response;
    if(r.op==0) {
        for(int i=0;i<MaxScopes;++i)if(scopes[i].token.load(std::memory_order_acquire)==0) {
            if(nextScope>=((uint64_t{1}<<55)-1)){response.status=S::Capacity;return response;}
            response.number=(nextScope++<<8)|static_cast<uint64_t>(i+1);
            scopes[i].token.store(response.number,std::memory_order_release);return response;
        }
        response.status=S::Capacity;return response;
    }
    if(!scopeLive(r.scope)){response.status=S::Expired;return response;}
    if(r.op==1){int id=0;response.status=bind(r,id);response.number=static_cast<uint64_t>(id);return response;}
    if(r.loadingRead&&r.op==2) {
        auto* b=binding(r.scope,r.member);
        if(!b||!b->isStatic){response.status=S::Expired;return response;}
        response.status=read(*b,0,response.values[0]);return response;
    }
    uintptr_t self=0;
    if(!universalHookMailbox().context(r.slot,r.ticket,&self)||self!=r.self){response.status=S::Expired;return response;}
    if(r.op==5||r.op==6) {
        if(r.count<1||r.count>MaxBatch){response.status=S::Invalid;return response;}
        Binding* methodBinding=nullptr;
        void* methodTarget=nullptr;
        alignas(16) uint8_t methodBytes[MaxArgs][16]{};
        void* methodArgs[MaxArgs]{};
        if(r.op==6) {
            methodBinding=binding(r.scope,r.member);
            if(!methodBinding){response.status=S::Expired;return response;}
            if(methodBinding->category!=3||r.callCount!=methodBinding->argc){
                response.status=S::Invalid;return response;
            }
            response.status=target(*methodBinding,self,methodTarget);
            if(response.status!=S::Ok)return response;
            for(int i=0;i<r.callCount;++i) {
                if(!encode(methodBinding->args[i],r.arguments[i],methodBytes[i])){
                    response.status=S::TypeMismatch;return response;
                }
                methodArgs[i]=methodBytes[i];
            }
        }
        for(int i=0;i<r.count;++i) {
            auto* b=binding(r.scope,r.ids[i]);
            if(!b){response.status=S::Expired;return response;}
            if(b->category!=1&&b->category!=2){response.status=S::Invalid;return response;}
            void* checked=nullptr;response.status=target(*b,self,checked);
            if(response.status!=S::Ok)return response;
        }
        for(int i=0;i<r.count;++i) {
            auto* item=binding(r.scope,r.ids[i]);
            if(!item){response.status=S::Expired;return response;}
            response.status=read(*item,self,response.values[i]);
            if(response.status!=S::Ok)return response;
        }
        if(methodBinding) {
            response.status=invoke(*methodBinding,methodBinding->getter,methodTarget,
                methodArgs,response.methodResult);
        }
        return response;
    }
    auto* b=binding(r.scope,r.member);
    if(!b){response.status=S::Expired;return response;}
    if(r.op==2){response.status=read(*b,self,response.values[0]);return response;}
    void* object=nullptr;response.status=target(*b,self,object);if(response.status!=S::Ok)return response;
    if(r.op==3) {
        if((b->category!=1&&b->category!=2)||!b->writable){response.status=S::ReadOnly;return response;}
        alignas(16) uint8_t bytes[16]{};
        if(!encode(b->value,r.values[0],bytes)){response.status=S::TypeMismatch;return response;}
        if(b->category==1) {
            if(b->isStatic) {
                if(!il2cpp::api().field_static_set_value){response.status=S::Unavailable;return response;}
                il2cpp::api().field_static_set_value(b->field,bytes);
            } else std::memcpy(reinterpret_cast<uint8_t*>(object)+b->offset,bytes,b->value.bytes);
        } else {
            Binding setter=*b;setter.value={};void* args[]={bytes};
            response.status=invoke(setter,b->setter,object,args,response.values[0]);
        }
        return response;
    }
    if(r.op==4) {
        if(b->category!=3||r.count!=b->argc){response.status=S::Invalid;return response;}
        alignas(16) uint8_t bytes[MaxArgs][16]{};void* args[MaxArgs]{};
        for(int i=0;i<r.count;++i) {
            if(!encode(b->args[i],r.values[i],bytes[i])){response.status=S::TypeMismatch;return response;}
            args[i]=bytes[i];
        }
        response.status=invoke(*b,b->getter,object,args,response.values[0]);return response;
    }
    response.status=S::Invalid;return response;
}
#if defined(BUNNY_NATIVE_ACCESS_HOST_TEST)
std::atomic<int> accessWaitMs{1000};
#else
constexpr int accessWaitMs=5;
#endif
Response submit(Request request) {
    using Clock=std::chrono::steady_clock;
    Response fail;
    if(!universalLeaveMono||!universalResumeMono){fail.status=S::WrongThread;return fail;}
    Clock::time_point deadline{};
    bool hookRequest=false;
    if(request.op<=1) {
        if(!universalCanInstall){fail.status=S::WrongThread;return fail;}
    } else if(request.op==2&&request.slot==-1&&request.ticket==0&&request.self==0&&universalCanInstall) {
        request.loadingRead=true;
    } else {
        uintptr_t self=0;
        if(!universalHookMailbox().workerDeadline(request.slot,request.ticket,&self,&deadline)||
           self!=request.self){fail.status=S::Expired;return fail;}
        hookRequest=true;
    }
    UniversalHookWorkerYield yield;
    // This is the Mono worker, after yielding Mono. An idle game-thread poll must
    // not reject a valid request; the game-thread pump keeps its nonblocking lock.
    std::unique_lock<std::mutex> lock(rpc.mutex);
    if(rpc.state!=Rpc::Idle){
        stats[12].fetch_add(1,std::memory_order_relaxed);
        fail.status=S::Busy;return fail;
    }
    const auto now=Clock::now();
    const auto limit=(request.op<=1||request.loadingRead)
        ?std::chrono::microseconds(2000000)
        :std::chrono::microseconds(accessWaitMs
#if defined(BUNNY_NATIVE_ACCESS_HOST_TEST)
            .load(std::memory_order_relaxed)
#endif
        *1000);
    auto budget=limit;
    if(hookRequest) {
        const auto remaining=std::chrono::duration_cast<std::chrono::microseconds>(deadline-now);
        constexpr auto reserve=std::chrono::microseconds(200);
        if(remaining<=reserve) {
            stats[6].fetch_add(1,std::memory_order_relaxed);
            fail.status=S::TimedOut;return fail;
        }
        if(budget>remaining-reserve)budget=remaining-reserve;
    }
    const auto completionBefore=rpc.completed.load(std::memory_order_relaxed);
    rpc.request=std::move(request);
    rpc.state=Rpc::Pending;
    universalHookMailbox().notify();
    if(hookRequest) {
        const auto spinUntil=std::min(Clock::now()+std::chrono::microseconds(48),
                                      now+budget);
        lock.unlock();
        while(Clock::now()<spinUntil &&
              rpc.completed.load(std::memory_order_acquire)==completionBefore) {}
        lock.lock();
        if(rpc.state==Rpc::Done) stats[13].fetch_add(1,std::memory_order_relaxed);
    }
    const bool done=rpc.state==Rpc::Done ||
        rpc.changed.wait_until(lock,now+budget,[]{return rpc.state==Rpc::Done;});
    const auto waited=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-now).count();
    stats[9].fetch_add(waited,std::memory_order_relaxed);
    auto peak=stats[11].load(std::memory_order_relaxed);
    while(waited>peak&&!stats[11].compare_exchange_weak(peak,waited,std::memory_order_relaxed)) {}
    if(!done||(hookRequest&&Clock::now()>=deadline)) {
        rpc.state=Rpc::Idle;
        stats[6].fetch_add(1,std::memory_order_relaxed);
        fail.status=S::TimedOut;return fail;
    }
    auto result=rpc.response;
    rpc.state=Rpc::Idle;
    return result;
}
}
int64_t nativeAccessStat(int key) {
    if((key>=0&&key<8)||(key>=9&&key<14))return stats[key].load(std::memory_order_relaxed);
    if(key==8){int count=0;for(auto& scope:scopes)if(scope.token.load(std::memory_order_relaxed))++count;return count;}
    return -1;
}
extern "C" __attribute__((visibility("default"))) int64_t bunny_native_access_stat(int key) { return nativeAccessStat(key); }
bool pumpNativeAccessOnGameThread() {
    std::unique_lock<std::mutex> lock(rpc.mutex,std::try_to_lock);
    // A producer notifies while publishing under this mutex. Do not consume that
    // wake as "idle" if it still owns the lock; the mailbox will retry within its deadline.
    if(!lock)return rpc.state.load(std::memory_order_acquire)==Rpc::Pending;
    if(rpc.state!=Rpc::Pending)return false;
    const bool previous=universalNativeAccessInProgress;
    universalNativeAccessInProgress=true;
    const auto begun=std::chrono::steady_clock::now();
    auto body=[&]{rpc.response=execute(rpc.request);};
    if(!il2cpp::guarded(body))rpc.response.status=S::NativeException;
    stats[10].fetch_add(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-begun).count(),std::memory_order_relaxed);
    universalNativeAccessInProgress=previous;
    const int status=static_cast<int>(rpc.response.status);
    stats[7].store(status,std::memory_order_relaxed);
    if(status==0&&rpc.request.op>=1&&rpc.request.op<=5)
        stats[rpc.request.op-1].fetch_add(1,std::memory_order_relaxed);
    else if(status==0&&rpc.request.op==6) {
        stats[3].fetch_add(1,std::memory_order_relaxed);
        stats[4].fetch_add(1,std::memory_order_relaxed);
    } else if(status!=0)stats[5].fetch_add(1,std::memory_order_relaxed);
    rpc.state=Rpc::Done;
    rpc.completed.fetch_add(1,std::memory_order_release);
    rpc.changed.notify_one();return true;
}
#if defined(BUNNY_NATIVE_ACCESS_HOST_TEST)
extern "C" void bunny_native_test_wait_ms(int value) { accessWaitMs.store(value,std::memory_order_relaxed); }
#endif
void resetNativeAccessOnGameThread() {
    for(auto& scope:scopes)scope.token.store(0,std::memory_order_release);
}
extern "C" __attribute__((visibility("default"))) int64_t bunny_native_open() {
    Request r;auto result=submit(std::move(r));return result.status==S::Ok?static_cast<int64_t>(result.number):static_cast<int>(result.status);
}
extern "C" __attribute__((visibility("default"))) void bunny_native_close(int64_t token) {
    const int slot=static_cast<int>(static_cast<uint64_t>(token)&255)-1;
    if(slot<0||slot>=MaxScopes)return;
    uint64_t expected=static_cast<uint64_t>(token);
    scopes[slot].token.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
}
extern "C" __attribute__((visibility("default"))) int bunny_native_bind(int64_t scope,int parent,int category,int kind,int flags,const char* name,const char* parameters) {
    if(!name||!parameters||strnlen(name,257)>256||strnlen(parameters,513)>512||category<0||category>3||kind<0||kind>10||flags<0||flags>3)return static_cast<int>(S::Invalid);
    if(category>0&&name[0]=='.')return static_cast<int>(S::Unsupported);
    Request r;r.op=1;r.scope=static_cast<uint64_t>(scope);r.parent=parent;r.category=category;r.kind=static_cast<K>(kind);r.flags=flags;r.name=name;r.parameters=parameters;
    auto result=submit(std::move(r));return result.status==S::Ok?static_cast<int>(result.number):static_cast<int>(result.status);
}
extern "C" __attribute__((visibility("default"))) int bunny_native_call(int64_t scope,int member,int op,int slot,int64_t ticket,uintptr_t self,int count,const AccessValue* input,AccessValue* output) {
    if(!output||count<0||count>MaxArgs||op<2||op>4||(count&&!input))return static_cast<int>(S::Invalid);
    Request r;r.scope=static_cast<uint64_t>(scope);r.member=member;r.op=op;r.slot=slot;r.ticket=static_cast<uint64_t>(ticket);r.self=self;r.count=count;
    for(int i=0;i<count;++i)r.values[i]=input[i];
    auto result=submit(std::move(r));
    if(result.status==S::Ok)*output=result.values[0];
    return static_cast<int>(result.status);
}
extern "C" __attribute__((visibility("default"))) int bunny_native_read_batch(int64_t scope,int slot,int64_t ticket,uintptr_t self,const int* members,AccessValue* output,int count) {
    if(!members||!output||count<1||count>MaxBatch)return static_cast<int>(S::Invalid);
    Request r;r.op=5;r.scope=static_cast<uint64_t>(scope);r.slot=slot;r.ticket=static_cast<uint64_t>(ticket);r.self=self;r.count=count;
    for(int i=0;i<count;++i)r.ids[i]=members[i];
    auto result=submit(std::move(r));
    if(result.status==S::Ok)for(int i=0;i<count;++i)output[i]=result.values[i];
    return static_cast<int>(result.status);
}

extern "C" __attribute__((visibility("default"))) int bunny_native_read_batch_invoke(
    int64_t scope,int slot,int64_t ticket,uintptr_t self,const int* members,AccessValue* output,
    int count,int methodId,int argCount,const AccessValue* input,AccessValue* methodOutput) {
    if(!members||!output||!methodOutput||count<1||count>MaxBatch||
       methodId<=0||argCount<0||argCount>MaxArgs||(argCount&&!input))
        return static_cast<int>(S::Invalid);
    Request r;r.op=6;r.scope=static_cast<uint64_t>(scope);
    r.slot=slot;r.ticket=static_cast<uint64_t>(ticket);r.self=self;
    r.count=count;r.member=methodId;r.callCount=argCount;
    for(int i=0;i<count;++i)r.ids[i]=members[i];
    for(int i=0;i<argCount;++i)r.arguments[i]=input[i];
    auto result=submit(std::move(r));
    if(result.status==S::Ok) {
        for(int i=0;i<count;++i)output[i]=result.values[i];
        *methodOutput=result.methodResult;
    }
    return static_cast<int>(result.status);
}
}
