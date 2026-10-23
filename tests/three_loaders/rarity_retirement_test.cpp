#include "experiments/csharp/NativeRarityGameplay.cpp"
#include <cassert>
#include <iostream>
#include <map>
#include <thread>
struct Il2CppClass {};
struct MethodInfo {int id;};
struct FieldInfo {};
struct Box {Il2CppObject object{};uint32_t value=0;};
bl::il2cpp::Api fixture;
std::map<int,uint32_t> colors;
Il2CppObject dictionary{};
Il2CppObject* dictionaryPtr=&dictionary;
MethodInfo contains{1},set{2},get{3},removeMethod{4};
FieldInfo dictionaryField;
Box booleanBox,colorBox;
bool failRemove=false,failGet=false,falseRemove=false;
const auto gameThread=std::this_thread::get_id();
namespace bl::il2cpp { Api& api(){return fixture;} }
namespace bl::csharp { void unloadManagedRarityCallbacks(const std::string&){} }
int main() {
 using namespace bl::csharp;
 fixture.field_static_get_value=[](FieldInfo*,void* output){assert(std::this_thread::get_id()==gameThread);std::memcpy(output,&dictionaryPtr,sizeof(dictionaryPtr));};
 fixture.object_get_class=[](Il2CppObject*)->Il2CppClass*{return nullptr;};
 fixture.runtime_invoke=[](const MethodInfo* method,void*,void** args,Il2CppObject** error)->Il2CppObject* {
  assert(std::this_thread::get_id()==gameThread);*error=nullptr;int key=*static_cast<int*>(args[0]);
  if(method==&contains) {booleanBox.value=colors.contains(key);return &booleanBox.object;}
  if(method==&set) {colors[key]=*static_cast<uint32_t*>(args[1]);return nullptr;}
  if(method==&get) {if(failGet){*error=&dictionary;return nullptr;}colorBox.value=colors.at(key);return &colorBox.object;}
  assert(method==&removeMethod);
  if(failRemove){*error=&dictionary;return nullptr;}
  booleanBox.value=falseRemove?0:colors.erase(key);return &booleanBox.object;
 };
 g_colorLayout.channels={0,1,2,3};g_table={&dictionaryField,&contains,&set,&get,&removeMethod,nullptr};
 int first=nativeRarities().registerType("one","R");
 int second=nativeRarities().registerType("two","R");
 assert(first==12&&second==13);
 assert(registerNativeRaritySampling("one","/one.dll"));
 assert(registerNativeRaritySampling("two","/two.dll"));
 assert(g_palette.publish(first,0xff123456)&&g_palette.publish(second,0xffabcdef));
 assert(writeRarityTable(first,0xff123456)&&writeRarityTable(second,0xffabcdef));
 const uint32_t otherColor=colors.at(second);
 std::thread retire([&]{retireNativeRaritySampling("one");});retire.join();
 uint32_t value=0;assert(!nativeManagedRarityColor(first,&value));
 assert(nativeManagedRarityColor(second,&value)&&value==0xffabcdef);
 assert(colors.contains(first)); // physical cleanup is deferred to the game thread
 failGet=true;retireRarityTableOnGameThread();assert(colors.contains(first)&&g_retiredTypes.contains(first));
 failGet=false;failRemove=true;retireRarityTableOnGameThread();assert(colors.contains(first));
 failRemove=false;falseRemove=true;retireRarityTableOnGameThread();assert(g_retiredTypes.contains(first));
 falseRemove=false;retireRarityTableOnGameThread();assert(!colors.contains(first)&&colors.at(second)==otherColor);
 assert(nativeRarities().registerType("one","R")==first);
 assert(registerNativeRaritySampling("one","/one.dll"));
 assert(writeRarityTable(first,0xff112233));
 colors[first]=0x12345678; // third-party replacement must survive update and unload
 assert(!writeRarityTable(first,0xff445566));
 retireNativeRaritySampling("one");retireRarityTableOnGameThread();assert(colors.at(first)==0x12345678);
 retireNativeRaritySampling("two");retireRarityTableOnGameThread();assert(!colors.contains(second));
 assert(g_mods.empty()&&g_retiredTypes.empty());
 assert(nativeRarities().snapshot()->size()==2);
 std::cout<<"RARITY_NATIVE_RELOAD_RESERVED_IDS_SELECTIVE_RETRY_AND_FOREIGN_CELLS_PASS\n";
}
