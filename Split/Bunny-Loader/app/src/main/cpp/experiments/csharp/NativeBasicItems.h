#pragma once
#include "experiments/csharp/BasicItemDefaults.h"
#include "experiments/csharp/SourceRecipeRecord.h"
#include "il2cpp/Types.h"
namespace bl::csharp {
bool checkBasicItemContract(std::string* error);
bool registerBasicItemPlaceholder(int type,const std::string& name,std::string* error,const std::string& modDir="",const std::string& sourceFile="");
bool applyNativeBasicItemDefaults(const std::string& modId,const std::vector<BasicItemEntry>& entries,std::string* error);
bool enqueueOwnedSourceRecipes(const std::string& modId,const std::vector<SourceRecipeOwner>& entries,std::string* error);
bool applyBasicItemToObject(Il2CppObject* item,int type);
bool isBasicItem(int type);
bool basicItemPolicyReady(int type);
struct BasicItemLoadStatus {
    int items=0;
    int pendingBehaviors=0;
    int readyBehaviors=0;
    int pendingRecipes=0;
    int installedRecipes=0;
    int pendingPlacements=0;
};
BasicItemLoadStatus basicItemLoadStatus();
bool basicItemHasDamage(int type);
bool basicItemIsAccessory(int type);
void applyBasicItemAccessoryEffects(Il2CppObject* player,int itemType);
bool setBasicItemDefense(int type,int value,std::string* error);
bool setBasicItemEquipSlot(int type,const std::string& kind,int slot,std::string* error);
std::vector<BasicItemEntry> pendingBasicItemBehaviors();
bool acknowledgeBasicItemBehavior(int type,const std::string& error);
std::vector<int> basicItemTypes();
int basicItemForTile(const std::string& key);
int basicItemForTileStyle(const std::string& key,int style);
int basicItemForWall(const std::string& key);
struct PendingBasicRecipe { std::string id,modId; int result=-1,index=-1; BasicRecipe values; std::string source="",error=""; };
std::vector<PendingBasicRecipe> pendingBasicRecipes();
bool acknowledgeBasicRecipe(const std::string& id,int index);
bool reportBasicRecipeError(const std::string& id,const std::string& error);
// Native game-update calls only if a batch is pending; no Mono or JS per frame.
bool hasManagedRecipeCallbacks(const std::string& modId);
void pumpBasicRecipesOnGameThread();
void requestBasicItemInstanceRefresh(const char* reason = "Unspecified");
}
