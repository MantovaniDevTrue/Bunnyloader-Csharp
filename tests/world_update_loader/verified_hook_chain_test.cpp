#include "hook/HookManager.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <cstdarg>
#include <thread>
#include <vector>
namespace { int installs=0;std::map<void*,void*> entries;bool failHook=false; }
void* shadowhook_hook_func_addr(void* address,void* replacement,void** original) {
 ++installs;if(failHook)return nullptr;
 entries[address]=replacement;*original=address;
 std::memset(address,0xcc,16);return address;
}
const char* shadowhook_to_errmsg(int){return "fixture failed install";}
int shadowhook_get_errno(){return 7;}
namespace bl::log {void write(int,const char*,...) {}}
int main(){
 // Real production HookManager; fake ShadowHook patches bytes exactly where
 // the second language used to compare patched code with vanilla opcodes.
 const std::array<unsigned char,16> proof{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
 std::array<std::array<unsigned char,16>,6> sites{};
 for(auto& site:sites)site=proof;
 int js=1,cs=2,third=3;void* originalsJs[2]{};void* originalsCs[2]{};void* originalCs=nullptr;
 for(int order=0;order<2;++order){
  void** first=order?&originalsCs[order]:&originalsJs[order];void** second=order?&originalsJs[order]:&originalsCs[order];
  void* firstHook=order?static_cast<void*>(&cs):&js;void* secondHook=order?static_cast<void*>(&js):&cs;
  auto& site=sites[order];int before=installs;
  assert(bl::hook::installVerified(site.data(),proof.data(),16,firstHook,first));
  assert(std::memcmp(site.data(),proof.data(),16)!=0);
  assert(bl::hook::installVerified(site.data(),proof.data(),16,secondHook,second));
  assert(entries[site.data()]==firstHook && *first==secondHook && *second==site.data());
  assert(installs==before+1);
  auto wrong=proof;wrong[0]=77;void* rejected=reinterpret_cast<void*>(0x123);
  assert(!bl::hook::installVerified(site.data(),wrong.data(),16,&third,&rejected));
  assert(!bl::hook::installVerified(site.data(),proof.data(),8,&third,&rejected));
  assert(rejected==reinterpret_cast<void*>(0x123) && *first==secondHook && *second==site.data());
 }
 void* out=nullptr;
 assert(bl::hook::install(sites[2].data(),&js,&out));
 assert(!bl::hook::installVerified(sites[2].data(),proof.data(),16,&cs,&originalCs));
 sites[3][5]=71;int before=installs;
 assert(!bl::hook::installVerified(sites[3].data(),proof.data(),16,&cs,&out));
 assert(installs==before);
 failHook=true;assert(!bl::hook::installVerified(sites[4].data(),proof.data(),16,&cs,&out));
 failHook=false;assert(bl::hook::installVerified(sites[4].data(),proof.data(),16,&cs,&out));
 assert(!bl::hook::installVerified(nullptr,proof.data(),16,&cs,&out));
 assert(!bl::hook::installVerified(sites[4].data(),proof.data(),0,&cs,&out));
 assert(!bl::hook::installVerified(sites[4].data(),proof.data(),65,&cs,&out));
 std::array<void*,16> links{};std::array<int,16> hooks{};std::vector<std::thread> workers;
 before=installs;
 for(size_t i=0;i<links.size();++i) workers.emplace_back([&,i]{assert(bl::hook::installVerified(sites[5].data(),proof.data(),16,&hooks[i],&links[i]));});
 for(auto& worker:workers)worker.join();
 assert(installs==before+1);void* current=entries[sites[5].data()];size_t count=0;
 while(current!=sites[5].data()) {
  size_t i=0;while(i<hooks.size() && current!=&hooks[i])++i;
  assert(i<hooks.size() && ++count<=hooks.size());current=links[i];
 }
 assert(count==hooks.size());
 std::cout<<"PRODUCTION_HOOK_MANAGER_JS_CS_BOTH_ORDERS_ORIGINAL_OPCODE_PROOF_AND_REJECTIONS=PASS\n";
}
