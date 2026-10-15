#include "il2cpp/ValueLayout.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>
struct Il2CppClass { const char* ns; const char* name; int size; bool value; std::vector<FieldInfo*> fields; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { Il2CppType type; size_t offset; unsigned flags=0; };
bl::il2cpp::Api nativeApi;
namespace bl::il2cpp {Api& api(){return nativeApi;}}
int main(){
 Il2CppClass single{"System","Single",4,true,{}},boolean{"System","Boolean",1,true,{}},
  text{"System","String",0,false,{}},point{"Game","Point",8,true,{}},
  inner{"Game","Inner",16,true,{}},outer{"Game","Outer",24,true,{}},
  ambiguous{"Game","Ambiguous",64,true,{}},mixed{"Game","Mixed",16,true,{}};
 FieldInfo x{{&single},sizeof(Il2CppObject)},y{{&single},sizeof(Il2CppObject)+4};point.fields={&x,&y};
 nativeApi.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 nativeApi.class_get_name=[](Il2CppClass* c){return c->name;};
 nativeApi.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 nativeApi.class_is_enum=[](const Il2CppClass*){return false;};
 nativeApi.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=8;return c->size;};
 nativeApi.class_get_fields=[](Il2CppClass* c,void** iter)->FieldInfo*{const auto i=reinterpret_cast<uintptr_t>(*iter);if(i>=c->fields.size())return nullptr;*iter=reinterpret_cast<void*>(i+1);return c->fields[i];};
 nativeApi.field_get_flags=[](FieldInfo* f){return f->flags;};
 nativeApi.field_get_type=[](FieldInfo* f){return static_cast<const Il2CppType*>(&f->type);};
 nativeApi.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 nativeApi.field_get_offset=[](FieldInfo* f){return f->offset;};
 bl::il2cpp::ValueLayout layout;std::string error;size_t offset=0;
 assert(bl::il2cpp::resolveValueLayout(&point,&layout,&error));assert(layout.header==sizeof(Il2CppObject));
 assert(layout.offset(y.offset,4,&offset)&&offset==4);assert(!layout.offset(y.offset,8,&offset));
 x.offset=0;y.offset=4;assert(bl::il2cpp::resolveValueLayout(&point,&layout,&error));assert(layout.header==0);
 x.offset=std::numeric_limits<size_t>::max();assert(!bl::il2cpp::resolveValueLayout(&point,&layout,&error));assert(error.find("FieldOutsideValue")!=std::string::npos);
 FieldInfo ambiguousField{{&single},sizeof(Il2CppObject)};ambiguous.fields={&ambiguousField};
 assert(!bl::il2cpp::resolveValueLayout(&ambiguous,&layout,&error));assert(error.find("AmbiguousFieldLayout")!=std::string::npos);
 FieldInfo a{{&boolean},0},b{{&text},24};mixed.fields={&a,&b};
 assert(!bl::il2cpp::resolveValueLayout(&mixed,&layout,&error));assert(error.find("FieldOutsideValue")!=std::string::npos);
 FieldInfo path{{&text},sizeof(Il2CppObject)+8},prefix{{&boolean},sizeof(Il2CppObject)},value{{&inner},sizeof(Il2CppObject)+8};
 inner.fields={&path};outer.fields={&prefix,&value};std::vector<size_t> references;
 assert(bl::il2cpp::valueReferenceOffsets(&outer,0,&references,&error));assert(references==std::vector<size_t>{16});
 FieldInfo staticField{{&text},std::numeric_limits<size_t>::max(),0x10};outer.fields.push_back(&staticField);
 assert(bl::il2cpp::resolveValueLayout(&outer,&layout,&error)); // Static fields do not occupy the value.
 Il2CppClass cycle{"Game","Cycle",16,true,{}};FieldInfo recursive{{&cycle},0};cycle.fields={&recursive};
 references.clear();assert(!bl::il2cpp::valueReferenceOffsets(&cycle,0,&references,&error));assert(error.find("ValueNestingExceeded")!=std::string::npos);
 assert(!bl::il2cpp::resolveValueLayout(&text,&layout,&error));
 std::cout<<"IL2CPP_NATIVE_VALUE_LAYOUT_NO_QUICKJS_PASS boxed,unboxed,bounds,ambiguity,mixed,nested-reference,static,cycle\n";
}
