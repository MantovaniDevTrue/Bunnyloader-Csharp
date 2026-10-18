#pragma once
#include "content/recipes/NativeRecipes.h"
#include "il2cpp/Types.h"
namespace bl::runtime {
enum class RecipeCallbackReadiness { Ready, Pending, Rejected };
struct RecipeCallbackProvider {
    RecipeCallbackReadiness (*prepare)(const NativeRecipeRequest&,std::string*)=nullptr;
    void (*track)(const NativeRecipeRequest&,Il2CppObject*,int)=nullptr;
};
// Provider has process lifetime. Installation is thread-safe; invocation is on the game thread.
void setRecipeCallbackProvider(const RecipeCallbackProvider* provider);
}
