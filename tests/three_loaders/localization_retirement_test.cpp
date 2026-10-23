// Exercise the actual publisher's owner/epoch entrypoints; no VM work on retire.
#include "experiments/csharp/NativeLocalizationRuntime.cpp"
#include <cassert>
#include <iostream>
int unloads=0;
namespace bl::log { void write(int,const char*,...) {} }
namespace bl::csharp {
void setManagedLocalizationResultSink(ManagedLocalizationResultSink) {}
void unloadManagedLocalizationCallbacks(const std::string&) {++unloads;}
bool queueManagedLocalizationLoaded(const std::string&,const std::string&,const std::string&,uint64_t) {return true;}
}
std::string hexString(const std::string& input) {
 const char* digits="0123456789ABCDEF";std::string result;
 for(unsigned char c:input){result+=digits[c>>4];result+=digits[c&15];}return result;
}
int main() {
 using namespace bl::csharp::localization;
 stage("one",{{"en-US","Mods.one.Name","Static"}});
 stage("two",{{"en-US","Mods.two.Name","Other"}});
 lastCulture="en-US";
 bindManagedAssembly("one","/same.dll|bunny-revision=1");
 bindManagedAssembly("two","/other.dll|bunny-revision=1");
 const auto old=managedEpochs.at("one");
 const std::string dynamic="OK|1\nBUNNY_DYNAMIC_LOCALES_V1\nLOCALE\ten-US\t"+hexString("Mods.one.Live")+"\t"+hexString("Old");
 assert(acceptManagedWire("one","/same.dll|bunny-revision=1","en-US",old,dynamic));
 retireMod("one");
 assert(unloads==1&&!managedAssemblies.contains("one"));
 assert(!acceptManagedWire("one","/same.dll|bunny-revision=1","en-US",old,dynamic));
 assert(session.snapshot().retired.contains("Mods.one.Live"));
 assert(session.snapshot().active.contains("two"));
 for(int i=0;i<100;++i)retireMod("one");
 assert(unloads==1); // idempotent retirement must not advance the worker floor
 stage("one",{{"en-US","Mods.one.Name","New"}});
 bindManagedAssembly("one","/same.dll|bunny-revision=1"); // even the same path receives a fresh epoch
 const auto fresh=managedEpochs.at("one");assert(fresh>old);
 assert(!acceptManagedWire("one","/same.dll|bunny-revision=1","en-US",old,dynamic));
 assert(!acceptManagedWire("one","/same.dll|bunny-revision=1","pt-BR",fresh,dynamic));
 assert(acceptManagedWire("one","/same.dll|bunny-revision=1","en-US",fresh,dynamic));
 retainActiveMods({"two"});assert(unloads==2&&session.snapshot().active.size()==1);
 retainActiveMods({});assert(unloads==3&&session.snapshot().active.empty());
 std::cout<<"LOCALIZATION_ACTUAL_RETIRE_SAME_PATH_STALE_EPOCH_AND_OTHER_OWNER_PASS\n";
}
