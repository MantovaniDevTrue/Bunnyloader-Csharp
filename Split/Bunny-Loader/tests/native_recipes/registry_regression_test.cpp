#include "content/items/ItemRegistryInternal.h"
#include "content/tiles/TileRegistryInternal.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using namespace bl::runtime;
int main(){
 ModItemDef item;item.mod="one";item.name="Bench";
 const auto first=registerModItem(item);assert(first==kVanillaItemCount);
 assert(registerModTile({"one","Bench","bench.png"})==kVanillaTileCount);
 assert(modItemKey(first)==modTileKey(kVanillaTileCount)); // Category keeps distinct numeric identities.
 item.mod="two";const auto second=registerModItem(item);assert(second==first+1);
 assert(modItemTypeByKey("one/Bench")==first&&modItemTypeByKey("two/Bench")==second);
 assert(registerModItem(item)==-1&&registerModTile({"one","Bench","other.png"})==-1);
 assert(modItemKey(-1).empty()&&modTileKey(753).empty());
 assert(modItemTypeByKey("missing/Bench")==-1&&modTileTypeByKey("missing/Bench")==-1);
 // The original unloaded pool must not satisfy a real mod dependency.
 item_registry::g_poolFirst=first+2;
 for(int i=0;i<kUnloadedPoolSize;i++){
  ModItemDef unloaded;unloaded.mod="loader";unloaded.name="Unloaded"+std::to_string(i);
  assert(registerModItem(unloaded)==item_registry::g_poolFirst+i);
 }
 assert(isUnloadedType(first+2)&&isUnloadedType(first+2+kUnloadedPoolSize-1));
 assert(!isUnloadedType(first)&&!isUnloadedType(first+2+kUnloadedPoolSize));
 item_registry::g_regs[2].unloadedKey="absent/Sword";
 assert(modItemKey(first+2)=="absent/Sword");assert(modItemKey(first+3).empty());
 assert(modItemTypeByKey("loader/Unloaded0")==-1&&modItemTypeByKey("absent/Sword")==-1);
 // Parallel C#/JS discovery cannot reserve two IDs for the same stable key.
 std::vector<std::thread> workers;
 for(int worker=0;worker<8;worker++)workers.emplace_back([]{
  for(int i=0;i<192;i++){
   ModItemDef def;def.mod="parallel";def.name="Item"+std::to_string(i);registerModItem(def);
   registerModTile({"parallel","Tile"+std::to_string(i),"asset.png"});
  }
 });
 for(auto& worker:workers)worker.join();
 assert(item_registry::g_total.load()==2+kUnloadedPoolSize+192);
 assert(tile_registry::g_total.load()==193);
 for(int i=0;i<192;i++){
  const auto key="parallel/Item"+std::to_string(i);const auto id=modItemTypeByKey(key);
  assert(id>=first+2+kUnloadedPoolSize&&modItemKey(id)==key);
  const auto tile="parallel/Tile"+std::to_string(i);const auto type=modTileTypeByKey(tile);
  assert(type>=kVanillaTileCount+1&&modTileKey(type)==tile);
 }
 item_registry::g_installed.store(item_registry::g_total.load());tile_registry::g_installed.store(tile_registry::g_total.load());
 assert(itemTypeCount()==first+2+kUnloadedPoolSize+192&&tileTypeCount()==kVanillaTileCount+193);
 std::cout<<"SHARED_REGISTRY_CATEGORY_KEYS_UNLOADED_POOL_AND_CONCURRENT_192_PASS\n";
}
