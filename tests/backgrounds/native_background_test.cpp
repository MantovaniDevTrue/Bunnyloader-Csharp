#include "il2cpp/Api.h"
#include <vector>
struct Il2CppClass { const char* ns; const char* name; int width; bool value; int rank; Il2CppClass* element=nullptr; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType type; uint32_t flags=0x10; Il2CppArray* array=nullptr; };
#include "content/backgrounds/BackgroundTextures.cpp"
#include "experiments/csharp/BackgroundTextureWire.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
bl::il2cpp::Api exports;
Il2CppClass integer{"System","Int32",4,true,0}, wide{"System","Int64",8,true,0};
Il2CppClass assetClass{"ReLogic.Content","Asset<Texture2D>",sizeof(void*),false,0};
Il2CppClass integers{"System","Int32[]",sizeof(void*),false,1,&integer};
Il2CppClass references{"ReLogic.Content","Asset<Texture2D>[]",sizeof(void*),false,1,&assetClass};
Il2CppClass mainClass{"Terraria","Main",0,false,0}, textures{"Terraria.GameContent","TextureAssets",0,false,0};
FieldInfo fieldAssets{"Background",{&references}}, fieldWidths{"backgroundWidth",{&integers}}, fieldHeights{"backgroundHeight",{&integers}}, fieldCount{"maxBackgrounds",{&integer}};
int gameTid=gettid(), loads=0, destroys=0, failResize=-1, resizes=0, writes=0;
std::map<uint32_t,Il2CppObject*> roots;
uint32_t nextRoot=1;
std::vector<void*> buffers;
Il2CppArray* makeArray(Il2CppClass* cls,size_t count) {
    const auto bytes=sizeof(Il2CppArray)+count*cls->element->width;
    auto* array=static_cast<Il2CppArray*>(std::calloc(1,bytes));
    array->obj.klass=cls;array->length=count;buffers.push_back(array);return array;
}
}
namespace bl::log { void write(int,const char*,...){} }
namespace bl::runtime { int gameThreadId(){return gameTid;} }
namespace bl::il2cpp {
Api& api(){return exports;}
Il2CppClass* findClass(const TypeRef& t){return t.name=="Main"?&mainClass:t.name=="TextureAssets"?&textures:nullptr;}
FieldInfo* findField(Il2CppClass*,std::string_view n){
    for(auto* f:{&fieldAssets,&fieldWidths,&fieldHeights,&fieldCount}) { if(n==f->name)return f; }
    return nullptr;
}
}
namespace bl::runtime {
Il2CppArray* TypeTables::resizedCopy(Il2CppArray* old,uintptr_t length){
    if(++resizes==failResize)return nullptr;
    auto* result=makeArray(old->obj.klass,length);
    std::memcpy(arrayData(result),arrayData(old),std::min(old->length,length)*old->obj.klass->element->width);
    return result;
}
namespace content {
Il2CppClass* textureAssetClass(){return &assetClass;}
Il2CppObject* loadTextureAsset(const std::string& path,const unsigned char*,size_t,const std::string&,int* w,int* h){
    ++loads;if(path.ends_with("invalid.png"))return nullptr;
    *w=640;*h=180;auto* asset=new Il2CppObject{&assetClass,nullptr};return asset;
}
void destroyTextureAsset(Il2CppObject* p){++destroys;delete p;}
}
}
void initializeExports() {
    exports.field_get_flags=[](FieldInfo* f){return f->flags;};
    exports.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
    exports.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
    exports.class_get_rank=[](const Il2CppClass* c){return c->rank;};
    exports.class_get_element_class=[](Il2CppClass* c){return c->element;};
    exports.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
    exports.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->width);};
    exports.class_get_name=[](Il2CppClass* c){return c->name;};
    exports.class_get_namespace=[](Il2CppClass* c){return c->ns;};
    exports.field_static_get_value=[](FieldInfo* f,void* p){if(f==&fieldCount)*static_cast<int*>(p)=3;else *static_cast<Il2CppArray**>(p)=f->array;};
    exports.field_static_set_value=[](FieldInfo* f,void* p){
        auto* array=static_cast<Il2CppArray*>(p);
        assert(array && array->obj.klass==f->type.cls);
        ++writes;f->array=array;
    };
    exports.gchandle_new=[](Il2CppObject* p,bool){uint32_t id=nextRoot++;roots[id]=p;return id;};
    exports.gchandle_free=[](uint32_t id){roots.erase(id);};
    exports.gchandle_get_target=[](uint32_t id){return roots.at(id);};
    exports.gc_wbarrier_set_field=[](Il2CppObject*,void** address,void* value){*address=value;};
}
extern "C" __attribute__((visibility("default"))) int bunny_test_background_init(const char* file) {
    initializeExports();
    fieldAssets.array=makeArray(&references,3);fieldWidths.array=makeArray(&integers,3);fieldHeights.array=makeArray(&integers,3);
    std::string error;
    if (bl::runtime::backgrounds::reserve("csharp-test-uid", "Assets/Textures/Backgrounds/ExampleBiomeSurfaceFar", file, &error) != 3) return -1;
    if (bl::runtime::backgrounds::reserve("second-test-uid", "Assets/Textures/Backgrounds/ExampleBiomeSurfaceFar", file, &error) != 4) return -2;
    return bl::runtime::backgrounds::publish(&error) ? 1 : -3;
}
extern "C" __attribute__((visibility("default"))) void bunny_test_background_unload(const char* mod) {
    bl::runtime::backgrounds::unload(mod);
}
int main(){
    using namespace bl::runtime::backgrounds;
    initializeExports();
    fieldAssets.array=makeArray(&references,3);fieldWidths.array=makeArray(&integers,3);fieldHeights.array=makeArray(&integers,3);
    const auto folder=std::filesystem::temp_directory_path()/"bunny-native-background-fixture";
    std::filesystem::remove_all(folder);std::filesystem::create_directories(folder/"Assets/Backgrounds");
    for(const char* n:{"far.png","mid.png","invalid.png"}) {std::ofstream out(folder/n);out<<"PNG-fixture-data";}
    std::filesystem::copy_file(folder/"far.png",folder/"Assets/Backgrounds/automatic.png");
    std::string error;
    assert(reserve("csharp-uid","Assets/Backgrounds/Far",(folder/"far.png").string(),&error)==3);
    assert(reserve("csharp-uid","Assets/Backgrounds/Far.png",(folder/"far.png").string(),&error)==3);
    assert(reserve("js-uid","Assets/Backgrounds/Mid",(folder/"mid.png").string(),&error)==4);
    assert(find("csharp-uid","Assets/Backgrounds/Far")==-1);
    assert(find("csharp-uid","Assets/Backgrounds/Far",false)==3);
    assert(reserve("bad/mod","Path",(folder/"far.png").string(),&error)==-1);
    assert(reserve("mod","../escape",(folder/"far.png").string(),&error)==-1);
    assert(reserve("mod","Missing",(folder/"missing.png").string(),&error)==-1);
    auto* original=fieldAssets.array;failResize=2;
    assert(!publish(&error)&&error.find("AllocationFailed")!=error.npos);
    assert(fieldAssets.array==original&&writes==0&&find("csharp-uid","Assets/Backgrounds/Far")==-1);
    failResize=-1;assert(publish(&error));assert(loads==2&&writes==3);
    assert(fieldAssets.array->length==5&&fieldWidths.array->length==5&&fieldHeights.array->length==5);
    assert(static_cast<int32_t*>(arrayData(fieldWidths.array))[3]==640);
    assert(static_cast<int32_t*>(arrayData(fieldHeights.array))[4]==180);
    assert(bunny_background_texture_slot("csharp-uid","Assets/Backgrounds/Far.png")==3);
    assert(bunny_background_texture_slot("js-uid","Assets/Backgrounds/Mid")==4);
    std::map<std::string,int> registered{{"BACKGROUND_TEXTURE:ExampleBackground",3}};
    std::map<std::string,bool> seen;
    std::set<std::string> rows;
    const std::vector<std::string> descriptor{"BACKGROUND_TEXTURE","ExampleBackground","Assets/Backgrounds/Far"};
    assert(bl::csharp::validateBackgroundTextureWire(descriptor,"csharp-uid",registered,&seen,&rows,&error));
    assert(seen.size()==1 && rows.size()==1 && seen.contains("BACKGROUND_TEXTURE:ExampleBackground"));
    assert(!bl::csharp::validateBackgroundTextureWire(descriptor,"csharp-uid",registered,&seen,&rows,&error));
    assert(seen.size()==1 && rows.size()==1);
    auto bad=descriptor;bad[1]="MissingClass";
    assert(!bl::csharp::validateBackgroundTextureWire(bad,"csharp-uid",registered,&seen,&rows,&error));

    std::thread other([&]{std::string e;assert(!publish(&e)&&e.find("WrongThread")!=e.npos);});other.join();
    assert(autoload("auto-uid",folder.string(),&error));assert(count()==3);assert(publish(&error));
    assert(find("auto-uid","Assets/Backgrounds/automatic")>=5);
    // Invalid IL2CPP element type cannot resize or write any table.
    original=fieldAssets.array;const int previousWrites=writes;integers.element=&wide;
    assert(!publish(&error)&&error.find("ArrayElementMismatch")!=error.npos);
    assert(fieldAssets.array==original&&writes==previousWrites);integers.element=&integer;
    assert(reserve("broken-uid","Broken",(folder/"invalid.png").string(),&error)>0);
    assert(!publish(&error)&&find("broken-uid","Broken")==-1);
    assert(readiness("broken-uid",&error)==-1 && error.find("Mod=broken-uid")!=error.npos);
    assert(publish(&error,"js-uid") && readiness("js-uid",&error)==1);
    assert(find("js-uid","Assets/Backgrounds/Mid")==4);
    unload("csharp-uid");assert(destroys==1&&find("csharp-uid","Assets/Backgrounds/Far")==-1);
    assert(static_cast<void**>(arrayData(fieldAssets.array))[3]==nullptr);
    assert(static_cast<int32_t*>(arrayData(fieldWidths.array))[3]==0);
    unload("js-uid");unload("auto-uid");unload("broken-uid");assert(destroys==3&&roots.empty());
    assert(reserve("csharp-uid","Assets/Backgrounds/Far",(folder/"far.png").string(),&error)>5);
    assert(publish(&error));unload("csharp-uid");
    for(auto* buffer:buffers)std::free(buffer);
    std::filesystem::remove_all(folder);
    std::cout<<"NATIVE_BACKGROUND_SHARED_REGISTRY_TYPED_TABLES_GC_TRANSACTION_ROLLBACK_AUTOLOAD_UNLOAD_MIXED_UIDS_NO_QUICKJS=PASS\n";
}
