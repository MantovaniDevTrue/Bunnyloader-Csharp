#pragma once
#include <string>
#include <vector>

namespace bl::runtime {
struct RecipeIngredient { int type=-1, count=1; std::string key; std::string nativeGroup{}; std::string customGroup{}, groupDescription{}; std::vector<std::string> groupMembers{}; };
struct NativeRecipeRequest {
    std::string key, mod, name, source;
    int result=-1, stack=1, station=-1;
    int callbacks=0;
    std::string stationKey;
    std::vector<RecipeIngredient> ingredients;
};
struct NativeRecipeResult { int index=-1; std::string error; };
// Game thread only. No script VM; keys come from the shared content registries.
// A registered recipe is acknowledged only after all Terraria lookup builders succeed.
NativeRecipeResult installNativeRecipe(const NativeRecipeRequest& request);
bool resolveNativeRecipe(const NativeRecipeRequest& request, NativeRecipeRequest* resolved, std::string* error);
}
