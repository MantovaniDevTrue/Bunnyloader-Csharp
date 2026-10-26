#include "il2cpp/FieldContract.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

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
Il2CppClass* alternateArrayClass=nullptr;
uint32_t reportedLength=4;
const Il2CppType* intArrayType=reinterpret_cast<const Il2CppType*>(0x20000);
const Il2CppType* boolArrayType=reinterpret_cast<const Il2CppType*>(0x30000);
const char* declaredTypeName(const Il2CppType* value) {
    return value==intArrayType?"System.Int32[]":"System.Boolean[]";
}
char* typeName(const Il2CppType* value) {
    return ::strdup(declaredTypeName(value));
}
void freeString(void* value) { std::free(value); }
Il2CppClass* fromType(const Il2CppType* value) {
    return value==intArrayType?&arrayClass:&wrongClass;
}
const Il2CppType* classType(Il2CppClass* cls) {
    return cls==&arrayClass||cls==alternateArrayClass?intArrayType:boolArrayType;
}
Il2CppClass* getObjectClass(Il2CppObject* obj) {
    return reinterpret_cast<Il2CppClass*>(obj->klass);
}
int elementSize(const Il2CppClass* cls) {
    return cls==&arrayClass||cls==alternateArrayClass?4:1;
}


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
    a.type_get_name=typeName;
    a.il2cpp_free=freeString;
    a.class_from_il2cpp_type=fromType;
    a.corlibImage=reinterpret_cast<const Il2CppImage*>(0x20000);
    a.class_from_name=[](const Il2CppImage*,const char* ns,const char* name)->Il2CppClass* {
        if(std::strcmp(ns,"System"))return nullptr;
        return !std::strcmp(name,"Int32")?&intClass:
               !std::strcmp(name,"Boolean")?&boolClass:nullptr;
    };
    a.array_class_get=arrayClassGet;
    a.object_get_class=getObjectClass;
    a.class_get_type=classType;
    a.array_element_size=elementSize;
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
    assert(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16)==nullptr);
    arrayClass.element=nullptr;
    assert(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16)==nullptr);
    assert(std::strcmp(bl::il2cpp::nativeDeclaredArrayIssue(&value,boolArrayType,"System.Int32[]",16),
                       "DeclaredArrayTypeMismatch")==0);
    reportedLength=3;
    assert(std::strcmp(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16),
                       "ArrayLengthAbiMismatch")==0);
    reportedLength=4;
    assert(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16)==nullptr);
    canonical=&arrayClass;
    value.obj.klass=reinterpret_cast<Il2CppClass*>(&wrongClass);
    assert(std::strcmp(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16),
                       "ArrayUnrecognizedRuntimeClass")==0);
    value.obj.klass=reinterpret_cast<Il2CppClass*>(&arrayClass);
    Il2CppClass duplicateArrayClass{"System","Int32[]",0,nullptr,false,0};
    alternateArrayClass=&duplicateArrayClass;
    value.obj.klass=reinterpret_cast<Il2CppClass*>(&duplicateArrayClass);
    canonical=&duplicateArrayClass;
    assert(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16)==nullptr);
    canonical=&arrayClass;
    assert(std::strcmp(bl::il2cpp::nativeDeclaredArrayIssue(&value,intArrayType,"System.Int32[]",16),
                       "ArrayUnrecognizedRuntimeClass")==0);
    value.obj.klass=reinterpret_cast<Il2CppClass*>(&arrayClass);
    alternateArrayClass=nullptr;
    std::puts("IL2CPP_NATIVE_ARRAY_CONTRACT=PASS");
}
