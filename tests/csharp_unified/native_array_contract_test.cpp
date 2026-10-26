#include "il2cpp/FieldContract.h"
#include <cassert>
#include <cstring>
#include <cstdio>

struct Il2CppClass {
    const char* ns;
    const char* name;
    int rank;
    Il2CppClass* element;
    bool valueType;
    int size;
};

namespace {
Il2CppClass intClass{"System","Int32",0,nullptr,true,4};
Il2CppClass boolClass{"System","Boolean",0,nullptr,true,1};
Il2CppClass arrayClass{"System","Int32[]",0,&intClass,false,0};
Il2CppClass wrongClass{"System","Object",0,&intClass,false,0};
Il2CppClass* canonical=&arrayClass;
uint32_t reportedLength=4;

int rank(const Il2CppClass* cls) { return cls->rank; }
Il2CppClass* element(Il2CppClass* cls) { return cls->element; }
const char* name(Il2CppClass* cls) { return cls->name; }
const char* ns(Il2CppClass* cls) { return cls->ns; }
bool valuetype(const Il2CppClass* cls) { return cls->valueType; }
int32_t size(Il2CppClass* cls,uint32_t*) { return cls->size; }
Il2CppClass* arrayClassGet(Il2CppClass* cls,uint32_t n) {
    return cls==&intClass && n==1 ? canonical : nullptr;
}
uint32_t arrayLength(Il2CppArray*) { return reportedLength; }
}

namespace bl::il2cpp {
Api& api() {
    static Api a{};
    return a;
}
}

int main() {
    auto& a=bl::il2cpp::api();
    a.class_get_rank=rank;
    a.class_get_element_class=element;
    a.class_get_name=name;
    a.class_get_namespace=ns;
    a.class_is_valuetype=valuetype;
    a.class_value_size=size;
    a.array_class_get=arrayClassGet;
    a.array_length=arrayLength;
    Il2CppArray value{{reinterpret_cast<::Il2CppClass*>(&arrayClass),nullptr},nullptr,4};
    auto test=[&](const char* expected) {
        const char* result=bl::il2cpp::nativeArrayContractIssue(&value,"Int32",4,16);
        assert(result && std::strcmp(result,expected)==0);
        assert(!bl::il2cpp::nativeArrayContract(&value,"Int32",4,16));
    };
    assert(bl::il2cpp::nativeArrayContractIssue(&value,"Int32",4,16)==nullptr);
    assert(bl::il2cpp::nativeArrayContract(&value,"Int32",4,16));
    canonical=&wrongClass;
    test("ArrayRankMismatchCanonicalClassMismatch");
    canonical=&arrayClass;
    reportedLength=3;
    test("ArrayLengthAbiMismatch");
    reportedLength=4;
    value.bounds=reinterpret_cast<Il2CppArrayBounds*>(0x10000);
    test("ArrayBoundsUnexpected");
    value.bounds=nullptr;
    arrayClass.element=&boolClass;
    test("ArrayElementTypeMismatch");
    arrayClass.element=&intClass;
    value.length=17;
    test("ArrayLengthOutOfBounds");
    value.length=4;
    arrayClass.rank=2;
    canonical=&wrongClass;
    test("ArrayRankMismatchCanonicalClassMismatch");
    arrayClass.rank=1;
    assert(bl::il2cpp::nativeArrayContract(&value,"Int32",4,16));
    arrayClass.element=reinterpret_cast<Il2CppClass*>(UINT64_C(0x2e6b726f77656d61));
    test("ArrayElementClassInvalidPointer");
    arrayClass.element=&intClass;
    assert(std::strcmp(bl::il2cpp::nativeArrayContractIssue(&value,"Int32",4,16,&wrongClass),
                       "ArrayRuntimeClassMismatch")==0);
    assert(bl::il2cpp::nativeArrayContractIssue(&value,"Int32",4,16,&arrayClass)==nullptr);
    std::puts("IL2CPP_NATIVE_ARRAY_CONTRACT=PASS");
}
