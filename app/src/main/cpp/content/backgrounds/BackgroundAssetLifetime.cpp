#include "content/backgrounds/BackgroundAssetLifetime.h"
#include "content/common/ContentAssets.h"
#include "il2cpp/Api.h"
#include "boot/Boot.h"
#include <array>
#include <map>
#include <set>
#include <mutex>
#include <limits>
#include <atomic>
#include <unistd.h>
namespace bl::runtime::backgroundAssets {
namespace {
struct Asset {uint32_t root=0,readers=0;bool retired=false;};
std::mutex mutex;
std::map<int,Asset> assets;
std::set<int> retiredSlots;
std::array<Retiring,16> observers{};
std::atomic<bool> pending{false};
}
bool adopt(int slot,uint32_t root) {
    if(slot<=0 || !root)return false;
    std::lock_guard lock(mutex);auto it=assets.find(slot);
    if(it!=assets.end())return !it->second.retired && it->second.root==root;
    if(retiredSlots.contains(slot))return false;
    assets.emplace(slot,Asset{root});return true;
}
bool retired(int slot){std::lock_guard lock(mutex);return retiredSlots.contains(slot);}
int retain(int slot) {
    std::lock_guard lock(mutex);auto it=assets.find(slot);
    if(it==assets.end() || it->second.retired || it->second.readers==std::numeric_limits<uint32_t>::max())return 0;
    ++it->second.readers;return slot;
}
void release(int slot) {
    if(!slot)return;
    {std::lock_guard lock(mutex);auto it=assets.find(slot);
     if(it==assets.end() || !it->second.readers)return;
     --it->second.readers;if(it->second.retired && !it->second.readers)pending.store(true,std::memory_order_release);}
    sweep();
}
void retire(int slot) {
    std::array<Retiring,16> listeners{};
    {std::lock_guard lock(mutex);auto it=assets.find(slot);if(it==assets.end() || it->second.retired)return;
     it->second.retired=true;retiredSlots.insert(slot);listeners=observers;pending.store(true,std::memory_order_release);}
    // No registry lock is held while a renderer marks its old layer unavailable.
    for(auto observer:listeners)if(observer)observer(slot);
    sweep();
}
bool observeRetirement(Retiring observer) {
    if(!observer)return false;
    std::lock_guard lock(mutex);
    for(auto existing:observers)if(existing==observer)return true;
    for(auto& entry:observers)if(!entry){entry=observer;return true;}
    return false;
}
void sweep() {
    if(!pending.load(std::memory_order_acquire) || gameThreadId()<=0 || gameThreadId()!=gettid())return;
    for(;;) {
        uint32_t root=0;
        {std::lock_guard lock(mutex);auto found=assets.end();
         for(auto it=assets.begin();it!=assets.end();++it)if(it->second.retired && !it->second.readers){found=it;break;}
         if(found==assets.end()){pending.store(false,std::memory_order_release);return;}
         root=found->second.root;assets.erase(found);}
        auto& a=il2cpp::api();auto* asset=a.gchandle_get_target(root);
        if(asset)content::destroyTextureAsset(asset);
        a.gchandle_free(root);
    }
}
}
