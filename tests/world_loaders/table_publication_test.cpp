#include "experiments/csharp/NativeManagedTablePublication.h"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <cstring>

namespace {
std::array<Il2CppArray*,6> slots{};
std::map<uint32_t,Il2CppObject*> handles;
uint32_t nextHandle=1;
int failedSlot=-1;
bool rollbackFails=false;
std::array<Il2CppArray,12> arrays{};
void get(FieldInfo* field,void* out) {
    const auto i=reinterpret_cast<uintptr_t>(field)-1;
    std::memcpy(out,&slots[i],sizeof(void*));
}
void set(FieldInfo* field,void* value) {
    const int i=static_cast<int>(reinterpret_cast<uintptr_t>(field)-1);
    if(i==failedSlot) {
        if(rollbackFails) slots[i]=&arrays[11];
        return;
    }
    slots[i]=static_cast<Il2CppArray*>(value);
}
uint32_t pin(Il2CppObject* object,bool pinned) {
    assert(pinned);auto h=nextHandle++;handles[h]=object;return h;
}
Il2CppObject* target(uint32_t handle) { return handles.at(handle); }
void freeRoot(uint32_t handle) { assert(handles.erase(handle)==1); }
struct Api {
    decltype(&get) field_static_get_value=get;
    decltype(&set) field_static_set_value=set;
    decltype(&pin) gchandle_new=pin;
    decltype(&target) gchandle_get_target=target;
    decltype(&freeRoot) gchandle_free=freeRoot;
};
}
int main() {
    using namespace bl::csharp::table_publication;
    Api api;
    std::array<FieldInfo*,6> fields{};
    std::array<Il2CppArray*,6> originals{},pending{};
    for(size_t i=0;i<6;i++) {
        fields[i]=reinterpret_cast<FieldInfo*>(i+1);
        originals[i]=&arrays[i];pending[i]=&arrays[i+6];
    }
    for(size_t count:{size_t{2},size_t{6}}) {
        slots=originals;
        {
            Roots roots(api);
            for(size_t i=0;i<count;i++)assert(roots.add(reinterpret_cast<Il2CppObject*>(pending[i])));
            assert(handles.size()==count);
            assert(publish(api,fields.data(),originals.data(),pending.data(),count,roots));
            for(size_t i=0;i<count;i++)assert(slots[i]==pending[i]);
        }
        assert(handles.empty());
        for(size_t failed=0;failed<count;failed++) {
            slots=originals;failedSlot=static_cast<int>(failed);
            { Roots roots(api);
              assert(roots.add(reinterpret_cast<Il2CppObject*>(pending[0])));
              assert(!publish(api,fields.data(),originals.data(),pending.data(),count,roots)); }
            assert(slots==originals);assert(handles.empty());
        }
        failedSlot=-1;
        slots=originals;slots[0]=pending[0];
        { Roots roots(api);assert(!publish(api,fields.data(),originals.data(),pending.data(),count,roots)); }
        assert(slots[0]==pending[0] && slots[1]==originals[1]);
    }
    slots=originals;failedSlot=1;rollbackFails=true;
    { Roots roots(api);
      assert(roots.add(reinterpret_cast<Il2CppObject*>(pending[0])));
      assert(!publish(api,fields.data(),originals.data(),pending.data(),2,roots)); }
    assert(handles.size()==1); // An inconclusive rollback must retain native roots.
    std::cout<<"TREE_GORE_STATIC_REFERENCE_PUBLICATION_GC_READBACK_ROLLBACK=PASS\n";
}
