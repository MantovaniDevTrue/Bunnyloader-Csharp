#include "experiments/csharp/NativeDustEmission.h"
#include "content/dusts/ModDusts.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/GuardedCall.h"
#include "core/Log.h"
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {
struct Request { uint64_t session; std::string mod, name; int player, count; };
std::mutex mutex;
std::deque<Request> requests;
uint64_t session = 0;
bool ready = false;
bool valid(const char* text, size_t limit) {
    if (!text) return false;
    const size_t size = strnlen(text, limit + 1);
    if (!size || size > limit) return false;
    for (size_t i=0;i<size;++i) {
        const char c=text[i];
        if (!(c>='a'&&c<='z') && !(c>='A'&&c<='Z') &&
            !(c>='0'&&c<='9') && c!='_' && c!='-') return false;
    }
    return true;
}
bool typeIs(const Il2CppType* type,const char* expected) {
    auto& api=il2cpp::api();
    char* name=type?api.type_get_name(type):nullptr;
    const bool result=name&&std::strcmp(name,expected)==0;
    if(name)api.il2cpp_free(name);
    return result;
}
struct Refs {
    FieldInfo* players=nullptr;
    FieldInfo* position=nullptr;
    FieldInfo* active=nullptr;
    const MethodInfo* spawn=nullptr;
    size_t positionOffset=0,activeOffset=0;
};
Refs refs;
bool resolve() {
    if(refs.spawn)return true;
    auto& api=il2cpp::api();
    auto* main=il2cpp::findClass({"Terraria","Main",{}});
    auto* player=il2cpp::findClass({"Terraria","Player",{}});
    auto* dust=il2cpp::findClass({"Terraria","Dust",{}});
    if(!main||!player||!dust)return false;
    Refs next;
    next.players=il2cpp::findField(main,"player");
    next.position=il2cpp::findField(player,"position");
    next.active=il2cpp::findField(player,"active");
    if(!next.players||!next.position||!next.active||
       !typeIs(api.field_get_type(next.players),"Terraria.Player[]")||
       !typeIs(api.field_get_type(next.position),"Microsoft.Xna.Framework.Vector2")||
       !typeIs(api.field_get_type(next.active),"System.Boolean"))return false;
    if((api.field_get_flags(next.players)&0x10u)==0 ||
       (api.field_get_flags(next.position)&0x10u)!=0 || (api.field_get_flags(next.active)&0x10u)!=0)return false;
    next.positionOffset=api.field_get_offset(next.position);next.activeOffset=api.field_get_offset(next.active);
    const size_t instanceSize=api.class_instance_size(player);
    uint32_t alignment=0;
    auto* positionClass=api.class_from_il2cpp_type(api.field_get_type(next.position));
    if(!positionClass||api.class_value_size(positionClass,&alignment)!=8||
       next.positionOffset<sizeof(Il2CppObject)||next.positionOffset+8>instanceSize||
       next.activeOffset<sizeof(Il2CppObject)||next.activeOffset+1>instanceSize)return false;
    constexpr const char* parameters[]={"Microsoft.Xna.Framework.Vector2","System.Int32","System.Int32","System.Int32","System.Single","System.Single","System.Int32","Microsoft.Xna.Framework.Color","System.Single"};
    void* iterator=nullptr;
    while(const MethodInfo* method=api.class_get_methods(dust,&iterator)) {
        if(std::strcmp(api.method_get_name(method),"NewDust")||api.method_get_param_count(method)!=9)continue;
        uint32_t flags=0;
        bool match=(api.method_get_flags(method,&flags)&0x10u)!=0 && typeIs(api.method_get_return_type(method),"System.Int32");
        for(unsigned i=0;i<9&&match;++i)match=typeIs(api.method_get_param(method,i),parameters[i]);
        if(match) { next.spawn=method;break; }
    }
    if(!next.spawn)return false;
    refs=next;
    return true;
}
}
void setNativeDustEmissionWorld(bool value,uint64_t epoch) {
    std::lock_guard lock(mutex);
    if(epoch!=session||!value)requests.clear();
    session=epoch;ready=value&&epoch!=0;
}
void pumpNativeDustEmission() {
    // Only the Terraria thread dereferences IL2CPP objects. Mono submits bounded values.
    for(int n=0;n<2;++n) {
        Request request;
        {
            std::lock_guard lock(mutex);
            if(!ready||requests.empty())return;
            request=std::move(requests.front());requests.pop_front();
            if(request.session!=session)continue;
        }
        const int type=runtime::modDustTypeByName(request.mod,request.name);
        if(type<runtime::kVanillaDustCount||!resolve()) {
            BL_WARN("[CSharp][WARN][DustEmission] Mod=%s Class=%s Stage=Resolve Result=Rejected Reason=%s",request.mod.c_str(),request.name.c_str(),type<runtime::kVanillaDustCount?"DustNotRegistered":"NativeContractUnavailable");
            continue;
        }
        auto& api=il2cpp::api();Il2CppArray* players=nullptr;
        api.field_static_get_value(refs.players,&players);
        if(!players||static_cast<uintptr_t>(request.player)>=players->length)continue;
        auto* player=static_cast<Il2CppObject**>(arrayData(players))[request.player];
        if(!player)continue;
        uint8_t active=0;std::memcpy(&active,reinterpret_cast<char*>(player)+refs.activeOffset,1);
        if(!active)continue;
        struct Vector2 { float x,y; } position{};
        std::memcpy(&position,reinterpret_cast<char*>(player)+refs.positionOffset,sizeof(position));
        if(!std::isfinite(position.x)||!std::isfinite(position.y))continue;
        int width=20,height=40,nativeType=type,alpha=0;float sx=0,sy=-1,scale=1;uint32_t color=0;
        void* args[]={&position,&width,&height,&nativeType,&sx,&sy,&alpha,&color,&scale};
        for(int i=0;i<request.count;++i) {
            Il2CppObject* exception=nullptr;api.runtime_invoke(refs.spawn,nullptr,args,&exception);
            if(exception) {
                BL_ERROR("[CSharp][ERROR][DustEmission] Mod=%s Class=%s Stage=NewDust Result=Interrupted Reason=NativeException Exception=%s",request.mod.c_str(),request.name.c_str(),il2cpp::describeException(exception).c_str());
                break;
            }
        }
    }
}
}
extern "C" __attribute__((visibility("default")))
int64_t bunny_dust_emission_session() {
    std::lock_guard lock(bl::csharp::mutex);
    return bl::csharp::ready?static_cast<int64_t>(bl::csharp::session):0;
}
extern "C" __attribute__((visibility("default")))
int bunny_dust_emit(int64_t epoch,const char* mod,const char* name,int player,int count) {
    using namespace bl::csharp;
    if(epoch<=0||!valid(mod,128)||!valid(name,72)||player<0||player>=255||count<1||count>16)return 0;
    std::lock_guard lock(mutex);
    if(!ready||session!=static_cast<uint64_t>(epoch)||requests.size()>=32)return 0;
    requests.push_back({session,mod,name,player,count});
    return 1;
}
