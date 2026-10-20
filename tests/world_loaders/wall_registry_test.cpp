#include "content/walls/ModWalls.h"
#include "content/tiles/ModTileMap.h"
#include <cassert>
#include <iostream>
namespace bl::log {void write(int,const char*,...) {}}
int main(){
 using namespace bl::runtime;
 const int csharp=registerModWall({"csharp","IndependentWall","wall.png"});
 const int js=registerModWall({"javascript","IndependentWall","other.png"});
 assert(csharp==367&&js==368&&isModWall(csharp)&&isModWall(js));
 std::string error;assert(registerModWall({"csharp","IndependentWall","duplicate.png"},&error)==-1&&error.find("DuplicateNativeWall")!=std::string::npos);
 assert(modWallTypeByName("csharp","IndependentWall")==csharp);
 assert(modWallTypeByKey("javascript/IndependentWall")==js);
 assert(modWallTypeByKey("missing/IndependentWall")==-1);
 assert(modWallKey(csharp)=="csharp/IndependentWall");
 assert(setModWallMapEntries(csharp,{{68,68,68,"Example Wall"},{20,20,20,"Example Other"}}));
 assert(!setModWallMapEntries(csharp,{{300,0,0,"Invalid"}}));
 assert(addModWallMapEntry(csharp,1,1,1,"Third")==2); // Invalid replace preserved both entries.
 assert(addModWallMapEntry(js,1,1,1,"JavaScript Wall")==0);
 assert(setModWallMapEntries(csharp,{{68,68,68,"Example Wall"}}));
 assert(addModWallMapEntry(csharp,1,1,1,"Second")==1); // Replace is idempotent, not append.
 assert(addModWallMapEntry(js,1,1,1,"Other JS Entry")==1); // Other owner unaffected.
 assert(!setModWallMapEntries(1,{{1,1,1,"Vanilla"}}));
 std::cout<<"WALL_REAL_SHARED_REGISTRY_KEYS_DUPLICATE_ATOMIC_MAP_REPLACE_JS_COEXISTENCE_NO_QUICKJS=PASS\n";
}
