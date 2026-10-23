#include "experiments/csharp/NativeDrinkParticleColors.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

struct Il2CppClass {
    const char* ns;const char* name;int bytes;bool value;int rank;Il2CppClass* element;
};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {const char* name;Il2CppType type;size_t offset;uint32_t flags;};

Il2CppClass byteType{"System","Byte",1,true,0,nullptr};
Il2CppClass colorType{"Microsoft.Xna.Framework","Color",4,true,0,nullptr};
Il2CppClass colorArray{"Microsoft.Xna.Framework","Color[]",0,false,1,&colorType};
Il2CppClass outerArray{"Microsoft.Xna.Framework","Color[][]",0,false,1,&colorArray};
Il2CppClass sets{"Terraria.ID","Sets",0,false,0,nullptr};
FieldInfo tableField{"DrinkParticleColors",{&outerArray},0,0x10};
FieldInfo colorFields[4]={{"R",{&byteType},2,0},{"G",{&byteType},1,0},
                          {"B",{&byteType},0,0},{"A",{&byteType},3,0}};
struct Table {
    Il2CppArray array;Il2CppArray* cells[32]{};
} table{{{&outerArray,nullptr},nullptr,32}};
bl::il2cpp::Api fixture;
std::map<uint32_t,Il2CppObject*> roots;
std::vector<std::unique_ptr<uint8_t[]>> allocations;
uint32_t nextHandle=1;int barriers=0;int reads=0;
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& ref) {
    return ref.ns=="Terraria.ID"&&ref.name=="ItemID"&&ref.nested=="Sets"?&sets:nullptr;
}
FieldInfo* findField(Il2CppClass* cls,std::string_view name) {
    if(cls==&sets&&name=="DrinkParticleColors")return &tableField;
    if(cls==&colorType)for(auto& f:colorFields)if(name==f.name)return &f;
    return nullptr;
}
}
int main() {
    fixture.class_from_il2cpp_type=[](const Il2CppType* p){return p->cls;};
    fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
    fixture.class_get_name=[](Il2CppClass* c){return c->name;};
    fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};
    fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
    fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    fixture.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=1;return c->bytes;};
    fixture.class_get_fields=[](Il2CppClass* c,void** iterator)->FieldInfo*{
        if(c!=&colorType)return nullptr;
        const auto index=reinterpret_cast<uintptr_t>(*iterator);
        if(index>=4)return nullptr;
        *iterator=reinterpret_cast<void*>(index+1);
        return &colorFields[index];
    };
    fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
    fixture.field_static_get_value=[](FieldInfo* f,void* output){
        assert(f==&tableField);++reads;
        Il2CppArray* current=&table.array;
        std::memcpy(output,&current,sizeof(current));
    };
    fixture.array_new=[](Il2CppClass* element,uintptr_t size)->Il2CppArray*{
        assert(element==&colorType&&size<=8);
        auto memory=std::make_unique<uint8_t[]>(sizeof(Il2CppArray)+size*4);
        std::memset(memory.get(),0,sizeof(Il2CppArray)+size*4);
        auto* array=reinterpret_cast<Il2CppArray*>(memory.get());
        array->obj.klass=&colorArray;array->length=size;
        allocations.emplace_back(std::move(memory));return array;
    };
    fixture.gchandle_new=[](Il2CppObject* target,bool){auto id=nextHandle++;roots[id]=target;return id;};
    fixture.gchandle_get_target=[](uint32_t id)->Il2CppObject*{
        auto found=roots.find(id);return found==roots.end()?nullptr:found->second;
    };
    fixture.gchandle_free=[](uint32_t id){assert(roots.erase(id)==1);};
    fixture.gc_wbarrier_set_field=[](Il2CppObject* parent,void** slot,void* value){
        assert(parent==reinterpret_cast<Il2CppObject*>(&table.array));
        ++barriers;*slot=value;
    };
    using namespace bl::csharp;
    std::string error;
    NativeDrinkColors a,b;
    assert(bindNativeDrinkColors(10,"FFF0F0F0,FFC8C8C8,FF8C8C8C",&a,&error));
    assert(a.rgba.size()==3&&a.rgba[0][0]==240&&a.rgba[1][1]==200&&a.rgba[2][2]==140);
    auto* original=fixture.array_new(&colorType,1);
    table.cells[10]=original;
    assert(applyNativeDrinkColors(a,10));
    assert(table.cells[10]!=original&&table.cells[10]->length==3);
    auto* raw=static_cast<uint8_t*>(arrayData(table.cells[10]));
    assert(raw[0]==240&&raw[1]==240&&raw[2]==240&&raw[3]==255);
    assert(raw[4]==200&&raw[5]==200&&raw[6]==200&&raw[7]==255);
    assert(raw[8]==140&&raw[9]==140&&raw[10]==140&&raw[11]==255);
    const int before=barriers,allocated=allocations.size();
    assert(applyNativeDrinkColors(a,10)&&barriers==before&&allocations.size()==static_cast<size_t>(allocated));
    assert(bindNativeDrinkColors(11,"FF102030",&b,&error));
    assert(applyNativeDrinkColors(b,11));
    auto* other=table.cells[11];
    assert(restoreNativeDrinkColors(a,10)&&table.cells[10]==original);
    assert(table.cells[11]==other&&roots.size()==1);
    assert(restoreNativeDrinkColors(b,11)&&table.cells[11]==nullptr);
    assert(roots.empty()&&g_nativeDrinkColors.empty());
    for(const auto& invalid:{"","FF000000,","FF00000","FFGG0000",
        "FF000000,FF000000,FF000000,FF000000,FF000000,FF000000,FF000000,FF000000,FF000000"}) {
        NativeDrinkColors broken;
        assert(!bindNativeDrinkColors(12,invalid,&broken,&error));
    }
    auto* originalClass=tableField.type.cls;
    tableField.type.cls=&colorArray;
    assert(!bindNativeDrinkColors(12,"FF000000",&b,&error));
    tableField.type.cls=originalClass;
    auto previousFlags=tableField.flags;tableField.flags=0;
    assert(!bindNativeDrinkColors(12,"FF000000",&b,&error));tableField.flags=previousFlags;
    auto previousSize=colorType.bytes;colorType.bytes=8;
    assert(!bindNativeDrinkColors(12,"FF000000",&b,&error));colorType.bytes=previousSize;
    assert(bindNativeDrinkColors(12,"FF000000",&b,&error));
    assert(applyNativeDrinkColors(b,12));
    auto* foreign=fixture.array_new(&colorType,1);
    table.cells[12]=foreign;
    assert(restoreNativeDrinkColors(b,12)&&table.cells[12]==foreign);
    assert(roots.empty());
    assert(applyNativeDrinkColors(a,10));
    assert(bindNativeDrinkColors(10,"FF0A141E",&b,&error));
    assert(applyNativeDrinkColors(b,10));
    assert(table.cells[10]!=original&&static_cast<uint8_t*>(arrayData(table.cells[10]))[0]==30);
    assert(restoreNativeDrinkColors(b,10)&&table.cells[10]==original);
    assert(roots.empty()&&g_nativeDrinkColors.empty());
    table.cells[10]=nullptr;
    assert(applyNativeDrinkColors(a,10));
    assert(restoreNativeDrinkColors(a,10)&&table.cells[10]==nullptr);
    assert(roots.empty()&&g_nativeDrinkColors.empty());
    std::cout<<"NATIVE_DRINK_PARTICLES=PASS barriers="<<barriers<<" metadataReads="<<reads<<"\n";
}
