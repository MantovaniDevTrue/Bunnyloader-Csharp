#include "experiments/csharp/NativeAccess.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "il2cpp/Api.h"
#include "il2cpp/GuardedCall.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <string>
#include <vector>

struct Il2CppType { Il2CppClass* cls;const char* name; };
struct FieldInfo { const char* name;Il2CppType* type;size_t offset;uint32_t flags;void* storage=nullptr; };
struct MethodInfo { const char* name;Il2CppType* result;std::vector<Il2CppType*> args;bool isStatic=false;bool generic=false;int operation=0; };
struct Il2CppClass { const char* ns;const char* name;int size;bool value=false;bool generic=false;Il2CppClass* parent=nullptr;std::vector<FieldInfo*> fields;std::vector<MethodInfo*> methods; };
struct Vector { float y,x; };
struct Color { uint8_t a,b,g,r; };
struct Rectangle { int w,h,x,y; };
struct Player { Il2CppObject head;int life;uint32_t mana;float speed;double timer;int64_t ticks;Vector position;Color color;Rectangle box;uint8_t prefix=7;int16_t small=-123;uint64_t wide=UINT64_MAX;uint8_t canary=0x5a; };
using V=bl::csharp::AccessValue;
using K=bl::csharp::AccessKind;
using S=bl::csharp::AccessStatus;
extern "C" void bunny_native_test_wait_ms(int);
extern "C" int64_t bunny_native_open();
extern "C" void bunny_native_close(int64_t);
extern "C" int bunny_native_bind(int64_t,int,int,int,int,const char*,const char*);
extern "C" int bunny_native_call(int64_t,int,int,int,int64_t,uintptr_t,int,const V*,V*);
extern "C" int bunny_native_read_batch(int64_t,int,int64_t,uintptr_t,const int*,V*,int);
extern "C" int bunny_native_read_batch_invoke(int64_t,int,int64_t,uintptr_t,const int*,V*,int,int,int,const V*,V*);
namespace {
bl::il2cpp::Api api;
Il2CppClass cVoid{"System","Void",0,true},cBool{"System","Boolean",1,true},cInt{"System","Int32",4,true},cUInt{"System","UInt32",4,true},cLong{"System","Int64",8,true},cFloat{"System","Single",4,true},cDouble{"System","Double",8,true},cByte{"System","Byte",1,true};
Il2CppClass cShort{"System","Int16",2,true},cWide{"System","UInt64",8,true};
Il2CppType tShort{&cShort,"System.Int16"},tWide{&cWide,"System.UInt64"};
Il2CppClass cVector{"Microsoft.Xna.Framework","Vector2",8,true},cColor{"Microsoft.Xna.Framework","Color",4,true},cRectangle{"Microsoft.Xna.Framework","Rectangle",16,true};
Il2CppClass cPlayer{"Terraria","Player",sizeof(Player)},cMain{"Terraria","Main",16},cOther{"Terraria","NPC",sizeof(Player)};
Il2CppType tVoid{&cVoid,"System.Void"},tBool{&cBool,"System.Boolean"},tInt{&cInt,"System.Int32"},tUInt{&cUInt,"System.UInt32"},tLong{&cLong,"System.Int64"},tFloat{&cFloat,"System.Single"},tDouble{&cDouble,"System.Double"},tByte{&cByte,"System.Byte"};
Il2CppType tVector{&cVector,"Microsoft.Xna.Framework.Vector2"},tColor{&cColor,"Microsoft.Xna.Framework.Color"},tRectangle{&cRectangle,"Microsoft.Xna.Framework.Rectangle"},tRefInt{&cInt,"System.Int32&"};
FieldInfo x{"X",&tFloat,20,0},y{"Y",&tFloat,16,0};
FieldInfo red{"R",&tByte,19,0},green{"G",&tByte,18,0},blue{"B",&tByte,17,0},alpha{"A",&tByte,16,0};
FieldInfo rx{"X",&tInt,24,0},ry{"Y",&tInt,28,0},rw{"Width",&tInt,16,0},rh{"Height",&tInt,20,0};
bool day=true;
FieldInfo life{"statLife",&tInt,offsetof(Player,life),0},mana{"mana",&tUInt,offsetof(Player,mana),0},speed{"speed",&tFloat,offsetof(Player,speed),0},timer{"timer",&tDouble,offsetof(Player,timer),0},ticks{"ticks",&tLong,offsetof(Player,ticks),0};
FieldInfo position{"position",&tVector,offsetof(Player,position),0},color{"color",&tColor,offsetof(Player,color),0},box{"box",&tRectangle,offsetof(Player,box),0},constant{"constant",&tInt,offsetof(Player,life),0x20},dayTime{"dayTime",&tBool,0,0x10,&day};
FieldInfo prefix{"prefix",&tByte,offsetof(Player,prefix),0},small{"small",&tShort,offsetof(Player,small),0},wide{"wide",&tWide,offsetof(Player,wide),0};
MethodInfo getLife{"get_Life",&tInt,{},false,false,1},setLife{"set_Life",&tVoid,{&tInt},false,false,2};
MethodInfo probeInt{"Probe",&tInt,{&tInt},false,false,3},probeFloat{"Probe",&tFloat,{&tFloat},false,false,4};
MethodInfo throwing{"Throw",&tVoid,{},false,false,5},refMethod{"Ref",&tVoid,{&tRefInt},false,false,6};
MethodInfo getReadOnly{"get_ReadOnly",&tInt,{},false,false,1};
Player player{{&cPlayer,nullptr},100,30,1.25f,3.5,9000000000LL,{8.5f,4.25f},{255,30,20,10},{50,60,10,20}};
std::thread::id gameThread;
std::mutex mono;
std::atomic<bool> stop{false};
struct Handles { int64_t scope=0;int type=0,life=0,mana=0,position=0,color=0,box=0,day=0,prop=0,method=0,floating=0,throwing=0,timer=0,ticks=0,speed=0,other=0,prefix=0,small=0,wide=0; } h;
V integer(int n) { return {K::Int32,0,static_cast<uint32_t>(n),0}; }
int call(int member,int op,const bl::csharp::UniversalHookMailbox::Work& work,V input={},V* out=nullptr,int count=0) {
    V result;return bunny_native_call(h.scope,member,op,work.slot,work.ticket,work.self,count,&input,out?out:&result);
}
}
namespace bl::il2cpp {
Api& api() { return ::api; }
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name) {
    for(auto* c:{&cPlayer,&cMain,&cOther})if(ns==c->ns&&name==c->name)return c;
    return nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    for(auto* c=cls;c;c=c->parent)for(auto* f:c->fields)if(name==f->name)return f;
    return nullptr;
}
bool guardedCall(void(*body)(void*),void* argument,Caught*) { body(argument);return true; }
}
int main() {
    using namespace bl::csharp;
    gameThread=std::this_thread::get_id();
    cVector.fields={&x,&y};cColor.fields={&red,&green,&blue,&alpha};cRectangle.fields={&rx,&ry,&rw,&rh};
    cPlayer.fields={&life,&mana,&speed,&timer,&ticks,&position,&color,&box,&constant,&prefix,&small,&wide};cMain.fields={&dayTime};cOther.fields={&life};
    cPlayer.methods={&getLife,&setLife,&probeInt,&probeFloat,&throwing,&refMethod,&getReadOnly};
    api.class_get_name=+[](Il2CppClass* c){return c->name;};
    api.class_get_namespace=+[](Il2CppClass* c){return c->ns;};
    api.class_get_parent=+[](Il2CppClass* c){return c->parent;};
    api.class_is_valuetype=+[](const Il2CppClass* c){return c->value;};
    api.class_is_generic=+[](const Il2CppClass* c){return c->generic;};
    api.class_is_enum=+[](const Il2CppClass*){return false;};
    api.class_instance_size=+[](Il2CppClass* c)->uint32_t{return c->size;};
    api.class_value_size=+[](Il2CppClass* c,uint32_t*)->int32_t{return c->size;};
    api.class_from_il2cpp_type=+[](const Il2CppType* t){return t->cls;};
    api.type_get_name=+[](const Il2CppType* t){return strdup(t->name);};api.il2cpp_free=free;
    api.field_get_type=+[](FieldInfo* f)->const Il2CppType*{return f->type;};
    api.field_get_flags=+[](FieldInfo* f){return f->flags;};
    api.field_get_offset=+[](FieldInfo* f){return f->offset;};
    api.class_get_fields=+[](Il2CppClass* c,void** iter)->FieldInfo* {
        size_t index=reinterpret_cast<size_t>(*iter);if(index>=c->fields.size())return nullptr;
        *iter=reinterpret_cast<void*>(index+1);return c->fields[index];
    };
    api.class_get_methods=+[](Il2CppClass* c,void** iter)->const MethodInfo* {
        size_t index=reinterpret_cast<size_t>(*iter);if(index>=c->methods.size())return nullptr;
        *iter=reinterpret_cast<void*>(index+1);return c->methods[index];
    };
    api.method_get_name=+[](const MethodInfo* m){return m->name;};
    api.method_is_instance=+[](const MethodInfo* m){return !m->isStatic;};
    api.method_is_generic=+[](const MethodInfo* m){return m->generic;};
    api.method_get_param_count=+[](const MethodInfo* m)->uint32_t{return m->args.size();};
    api.method_get_param=+[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return m->args.at(i);};
    api.method_get_return_type=+[](const MethodInfo* m)->const Il2CppType*{return m->result;};
    api.object_get_class=+[](Il2CppObject* o){return o->klass;};
    api.class_is_assignable_from=+[](Il2CppClass* parent,Il2CppClass* child){return parent==child;};
    api.field_static_get_value=+[](FieldInfo* f,void* out){assert(std::this_thread::get_id()==gameThread);std::memcpy(out,f->storage,f->type->cls->size);};
    api.field_static_set_value=+[](FieldInfo* f,void* in){assert(std::this_thread::get_id()==gameThread);std::memcpy(f->storage,in,f->type->cls->size);};
    api.runtime_invoke=+[](const MethodInfo* m,void* self,void** args,Il2CppObject** exception)->Il2CppObject* {
        assert(std::this_thread::get_id()==gameThread);
        alignas(16) static uint8_t bytes[32]{};
        auto* header=reinterpret_cast<Il2CppObject*>(bytes);header->klass=m->result->cls;header->monitor=nullptr;
        auto* target=static_cast<Player*>(self);int result=0;
        if(m->operation==1)result=target->life;
        if(m->operation==2){int value=*static_cast<int*>(args[0]);if(value<0){*exception=header;return nullptr;}target->life=value;return nullptr;}
        if(m->operation==3)result=*static_cast<int*>(args[0])+1;
        if(m->operation==4){float value=*static_cast<float*>(args[0])*2;std::memcpy(bytes+16,&value,4);return header;}
        if(m->operation==5){*exception=header;return nullptr;}
        std::memcpy(bytes+16,&result,4);return header;
    };
    assert(bunny_native_open()==static_cast<int>(S::WrongThread));
    std::promise<void> registered;
    auto ready=registered.get_future();
    std::atomic<bool> workWaiting{false}, workerIdle{true};
    auto wakeWorker=[&]{ workerIdle.store(false,std::memory_order_release);workWaiting.store(true,std::memory_order_release); };
    auto waitWorkerIdle=[&]{
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!workerIdle.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<deadline)
            std::this_thread::yield();
        assert(workerIdle.load(std::memory_order_acquire));
    };
    std::thread worker([&]{
        mono.lock();universalLeaveMono=+[]{mono.unlock();};universalResumeMono=+[]{mono.lock();};universalCanInstall=true;
        h.scope=bunny_native_open();assert(h.scope>0);
        auto bind=[&](int parent,int category,K kind,int flags,const char* name,const char* args="") {
            return bunny_native_bind(h.scope,parent,category,static_cast<int>(kind),flags,name,args);
        };
        h.type=bind(0,0,K::Void,0,"Terraria.Player");assert(h.type>0);
        h.prefix=bind(h.type,1,K::UInt32,2,"prefix");h.small=bind(h.type,1,K::Int32,2,"small");h.wide=bind(h.type,1,K::UInt64,2,"wide");
        h.life=bind(h.type,1,K::Int32,2,"statLife");h.mana=bind(h.type,1,K::UInt32,0,"mana");
        h.position=bind(h.type,1,K::Vector2,2,"position");h.color=bind(h.type,1,K::Color,2,"color");h.box=bind(h.type,1,K::Rectangle,2,"box");
        h.speed=bind(h.type,1,K::Single,0,"speed");h.timer=bind(h.type,1,K::Double,0,"timer");h.ticks=bind(h.type,1,K::Int64,0,"ticks");
        int main=bind(0,0,K::Void,0,"Terraria.Main");h.day=bind(main,1,K::Boolean,3,"dayTime");
        h.prop=bind(h.type,2,K::Int32,2,"Life");h.method=bind(h.type,3,K::Int32,0,"Probe","System.Int32");
        h.floating=bind(h.type,3,K::Single,0,"Probe","System.Single");h.throwing=bind(h.type,3,K::Void,0,"Throw");
        int other=bind(0,0,K::Void,0,"Terraria.NPC");h.other=bind(other,1,K::Int32,0,"statLife");
        for(int id:{h.life,h.mana,h.position,h.color,h.box,h.speed,h.timer,h.ticks,h.day,h.prop,h.method,h.floating,h.throwing,h.other})assert(id>0);
        assert(bind(h.type,1,K::Int32,2,"constant")==static_cast<int>(S::ReadOnly));
        assert(bind(h.type,2,K::Int32,2,"ReadOnly")==static_cast<int>(S::ReadOnly));
        assert(bind(h.type,1,K::Double,0,"statLife")==static_cast<int>(S::TypeMismatch));
        assert(bind(h.type,1,K::Int32,0,"Missing")==static_cast<int>(S::NotFound));
        assert(bind(h.type,3,K::Void,0,"Ref","System.Int32&")==static_cast<int>(S::Unsupported));
        V initialDay;
        assert(bunny_native_call(h.scope,h.day,2,-1,0,0,0,nullptr,&initialDay)==0&&initialDay.a==1);
        assert(bunny_native_call(h.scope,h.life,2,-1,0,0,0,nullptr,&initialDay)==static_cast<int>(S::Expired));
        universalCanInstall=false;mono.unlock();registered.set_value();
        while(!stop) {
            if(!workWaiting.exchange(false,std::memory_order_acq_rel)){std::this_thread::yield();continue;}
            UniversalHookMailbox::Work work{};
            assert(universalHookMailbox().take(work));
            mono.lock();
            if(work.slot==6) {
                assert(call(h.life,3,work,integer(999),nullptr,1)==static_cast<int>(S::TimedOut));
                universalHookMailbox().complete(work.ticket,true);mono.unlock();workerIdle.store(true,std::memory_order_release);continue;
            }
            V result{};
            if(work.slot==7) {
                assert(work.int32Mode&&work.argument==9);
                int ids[]={h.life,h.day};V values[2]{};
                V arg=integer(work.argument);
                assert(bunny_native_read_batch_invoke(h.scope,work.slot,work.ticket,work.self,
                    ids,values,2,h.method,1,&arg,&result)==0&&values[0].a==125&&result.a==10);
                int32_t originalValue=0;
                assert(universalHookMailbox().originalInt32(work.slot,work.ticket,originalValue)&&originalValue==42);
                universalHookMailbox().complete(work.ticket,true,originalValue+static_cast<int>(result.a));
                mono.unlock();workerIdle.store(true,std::memory_order_release);continue;
            }
            const int prefixStatus=call(h.prefix,2,work,{},&result);
            if(prefixStatus!=0||result.a!=7)
                std::cerr<<"prefix read: status="<<prefixStatus<<" expectedStatus=0 actual="<<result.a
                         <<" expected=7 slot="<<work.slot<<" ticket="<<work.ticket<<"\n";
            assert(prefixStatus==0&&result.a==7);
            assert(call(h.prefix,3,work,{K::UInt32,0,255,0},nullptr,1)==0);
            assert(call(h.prefix,3,work,{K::UInt32,0,256,0},nullptr,1)==static_cast<int>(S::TypeMismatch));
            assert(call(h.small,2,work,{},&result)==0&&static_cast<int32_t>(result.a)==-123);
            assert(call(h.small,3,work,integer(-32768),nullptr,1)==0);
            assert(call(h.small,3,work,integer(32768),nullptr,1)==static_cast<int>(S::TypeMismatch));
            assert(call(h.wide,2,work,{},&result)==0&&result.a==UINT64_MAX);
            assert(call(h.life,2,work,{},&result)==0&&result.a==100);
            assert(call(h.life,3,work,integer(123),nullptr,1)==0);
            assert(call(h.prop,2,work,{},&result)==0&&result.a==123);
            assert(call(h.prop,3,work,integer(125),nullptr,1)==0);
            assert(call(h.prop,3,work,integer(-1),nullptr,1)==static_cast<int>(S::NativeException));
            assert(call(h.method,4,work,integer(4),&result,1)==0&&result.a==5);
            V f{K::Single,0,0,0};float three=3;std::memcpy(&f.a,&three,4);
            assert(call(h.floating,4,work,f,&result,1)==0);float six=0;std::memcpy(&six,&result.a,4);assert(six==6);
            assert(call(h.throwing,4,work)==static_cast<int>(S::NativeException));
            assert(call(h.other,2,work)==static_cast<int>(S::TypeMismatch));
            assert(call(h.mana,3,work,{K::UInt32,0,55,0},nullptr,1)==static_cast<int>(S::ReadOnly));
            assert(call(h.life,3,work,f,nullptr,1)==static_cast<int>(S::TypeMismatch));
            assert(call(h.position,2,work,{},&result)==0);
            float px=0,py=0;std::memcpy(&px,&result.a,4);uint32_t high=result.a>>32;std::memcpy(&py,&high,4);assert(px==4.25f&&py==8.5f);
            assert(call(h.color,2,work,{},&result)==0&&result.a==0xff1e140a);
            assert(call(h.color,3,work,{K::Color,0,0x04030201,0},nullptr,1)==0);
            assert(call(h.box,2,work,{},&result)==0&&static_cast<int>(result.a)==10&&static_cast<int>(result.a>>32)==20&&static_cast<int>(result.b)==50);
            assert(call(h.day,3,work,{K::Boolean,0,0,0},nullptr,1)==0);
            int ids[]={h.life,h.position,h.color,h.box,h.day,h.speed,h.timer,h.ticks};V values[8]{};
            assert(bunny_native_read_batch(h.scope,work.slot,work.ticket,work.self,ids,values,8)==0);
            assert(values[0].a==125&&values[2].a==0x04030201&&values[4].a==0&&values[7].a==9000000000ULL);
            int fusedIds[]={h.life,h.day};
            V fusedValues[2]{};V fusedArg=integer(12);V fusedResult{};
            assert(bunny_native_read_batch_invoke(h.scope,work.slot,work.ticket,work.self,
                fusedIds,fusedValues,2,h.method,1,&fusedArg,&fusedResult)==0);
            assert(fusedValues[0].a==125&&fusedResult.a==13);
            V fusedUntouched[2]={integer(555),integer(555)};
            V wrongArg={K::Single,0,0,0};
            assert(bunny_native_read_batch_invoke(h.scope,work.slot,work.ticket,work.self,
                fusedIds,fusedUntouched,2,h.method,1,&wrongArg,&fusedResult)==static_cast<int>(S::TypeMismatch));
            assert(fusedUntouched[0].a==555&&fusedUntouched[1].a==555);
            V sentinel{K::Int32,0,987,0};int invalid[]={h.life,2048};V untouched[]={sentinel,sentinel};
            assert(bunny_native_read_batch(h.scope,work.slot,work.ticket,work.self,invalid,untouched,2)==static_cast<int>(S::Expired)&&untouched[0].a==987);
            assert(bunny_native_call(h.scope,h.life,2,work.slot,work.ticket+1,work.self,0,nullptr,&result)==static_cast<int>(S::Expired));
            assert(bunny_native_call(h.scope,h.life,2,work.slot,work.ticket,work.self+1,0,nullptr,&result)==static_cast<int>(S::Expired));
            assert(universalHookMailbox().original(work.slot,work.ticket));
            universalHookMailbox().complete(work.ticket,true);
            assert(call(h.life,2,work)==static_cast<int>(S::Expired));
            mono.unlock();
            workerIdle.store(true,std::memory_order_release);
        }
        universalLeaveMono=universalResumeMono=nullptr;
    });
    while(ready.wait_for(std::chrono::milliseconds(1))!=std::future_status::ready)pumpNativeAccessOnGameThread();
    int originals=0;
    bunny_native_test_wait_ms(5);
    auto cancelled=universalHookMailbox().dispatch(6,reinterpret_cast<uintptr_t>(&player),[]{return true;},wakeWorker,std::chrono::seconds(1));
    assert(cancelled.accepted&&!cancelled.faulted&&!cancelled.originalRan&&player.life==100&&!pumpNativeAccessOnGameThread());
    waitWorkerIdle();
    bunny_native_test_wait_ms(1000);
    auto result=universalHookMailbox().dispatch(5,reinterpret_cast<uintptr_t>(&player),[&]{++originals;return true;},wakeWorker,std::chrono::seconds(5),0,false,pumpNativeAccessOnGameThread);
    if(!result.accepted||result.faulted||!result.originalRan)
        std::cerr<<"positive RPC dispatch: accepted="<<result.accepted<<" faulted="<<result.faulted
                 <<" timedOut="<<result.timedOut<<" originalRan="<<result.originalRan<<"\n";
    assert(result.accepted&&!result.faulted&&result.originalRan&&originals==1&&player.life==125&&!day&&player.color.r==1&&player.color.a==4&&player.prefix==255&&player.small==-32768&&player.canary==0x5a);
    waitWorkerIdle();
    auto typed=universalHookMailbox().dispatch(7,reinterpret_cast<uintptr_t>(&player),[&]{++originals;universalHookMailbox().setOriginalInt32Value(42);return true;},wakeWorker,std::chrono::seconds(5),9,true,pumpNativeAccessOnGameThread);
    assert(typed.accepted&&!typed.faulted&&typed.originalRan&&typed.value==52&&originals==2);
    waitWorkerIdle();
    stop=true;worker.join();
    auto recycled=std::async(std::launch::async,[&] {
        mono.lock();universalLeaveMono=+[]{mono.unlock();};universalResumeMono=+[]{mono.lock();};universalCanInstall=true;
        bunny_native_close(h.scope);
        const int64_t fresh=bunny_native_open();assert(fresh>0&&fresh!=h.scope);
        bunny_native_close(h.scope);
        assert(bunny_native_bind(fresh,0,0,0,0,"Terraria.Player","")>0);
        assert(bunny_native_bind(h.scope,0,0,0,0,"Terraria.Player","")==static_cast<int>(S::Expired));
        bunny_native_close(fresh);mono.unlock();
    });
    while(recycled.wait_for(std::chrono::milliseconds(1))!=std::future_status::ready)pumpNativeAccessOnGameThread();
    recycled.get();resetNativeAccessOnGameThread();
    std::cout<<"Native access: game-thread RPC, scoped self, fields/properties/methods, scalar/struct layouts, batch publish, expiry and exception PASS\n";
}
