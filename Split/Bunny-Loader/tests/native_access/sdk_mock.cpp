#include "experiments/csharp/NativeAccess.h"
#include <array>
#include <cstring>
using V=bl::csharp::AccessValue;
using K=bl::csharp::AccessKind;
struct Member { int category=0;K kind=K::Void;bool writable=false; };
std::array<Member,100> members{};
int next=1,lastSlot=0,calls=0,closes=0,batchFailure=0;
V stored{K::Int32,0,100,0};
extern "C" {
int bunny_universal_hook_install(const char*,const char*,int slot,int) { lastSlot=slot;return slot; }
int bunny_universal_hook_install_i32(const char*,const char*,int slot,int) { lastSlot=slot;return slot; }
int bunny_universal_hook_original_i32(int,int64_t,int32_t* result) { *result=42;return 1; }
void bunny_universal_hook_remove(int) { }
int bunny_universal_hook_original(int,int64_t) { return 1; }
int64_t bunny_universal_hook_stat(int) { return 0; }
int64_t bunny_native_access_stat(int) { return 0; }
int64_t bunny_native_open() { return 257; }
void bunny_native_close(int64_t) { ++closes; }
int bunny_native_bind(int64_t,int,int category,int kind,int flags,const char* name,const char*) {
    if(!std::strcmp(name,"Missing"))return -3;
    int id=next++;members[id]={category,static_cast<K>(kind),(flags&2)!=0};return id;
}
V value(int id) {
    V result;result.kind=members[id].kind;
    if(result.kind==K::Int32)return stored;
    if(result.kind==K::Boolean)result.a=1;
    if(result.kind==K::Vector2) { float x=4,y=8;uint32_t first=0,second=0;std::memcpy(&first,&x,4);std::memcpy(&second,&y,4);result.a=first|(static_cast<uint64_t>(second)<<32); }
    return result;
}
int bunny_native_call(int64_t,int member,int op,int,int64_t,uintptr_t,int,const V* input,V* output) {
    ++calls;
    if(op==3){if(!members[member].writable)return -6;if(input[0].kind!=members[member].kind)return -5;stored=input[0];*output={};return 0;}
    *output=value(member);
    if(op==4)*output={K::Int32,0,input[0].a+1,0};
    return 0;
}
int bunny_native_read_batch(int64_t,int,int64_t,uintptr_t,const int* ids,V* output,int count) {
    ++calls;
    for(int i=0;i<count;++i)output[i]=value(ids[i]);
    return batchFailure?-10:0;
}
int bunny_native_read_batch_invoke(int64_t,int,int64_t,uintptr_t,const int* ids,V* output,
    int count,int method,int argCount,const V* input,V* methodOut) {
    ++calls;
    if(batchFailure||argCount!=1||members[method].category!=3)return -10;
    for(int i=0;i<count;++i)output[i]=value(ids[i]);
    *methodOut={K::Int32,0,input[0].a+1,0};
    return 0;
}
int test_slot() { return lastSlot; }
int test_calls() { return calls; }
int test_closes() { return closes; }
void test_batch_fail(int value) { batchFailure=value; }
}
