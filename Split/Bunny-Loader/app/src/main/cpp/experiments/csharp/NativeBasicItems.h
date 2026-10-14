#pragma once
#include "experiments/csharp/BasicItemDefaults.h"
#include "il2cpp/Types.h"
namespace bl::csharp {
bool registerBasicItemPlaceholder(int type,const std::string& name,std::string* error);
bool applyNativeBasicItemDefaults(const std::string& modId,const std::vector<BasicItemEntry>& entries,std::string* error);
bool applyBasicItemToObject(Il2CppObject* item,int type);
bool isBasicItem(int type);
bool basicItemHasDamage(int type);
std::vector<int> basicItemTypes();
struct PendingBasicRecipe { std::string id,modId; int result=-1,index=-1; BasicRecipe values; };
std::vector<PendingBasicRecipe> pendingBasicRecipes();
bool acknowledgeBasicRecipe(const std::string& id,int index);
// Native game-update calls only if a batch is pending; no Mono or JS per frame.
void pumpBasicRecipesOnGameThread();
}
