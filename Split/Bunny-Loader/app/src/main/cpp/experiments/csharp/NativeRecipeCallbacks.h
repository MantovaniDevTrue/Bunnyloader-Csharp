#pragma once
#include "content/recipes/NativeRecipes.h"
#include "content/recipes/RecipeCallbackProvider.h"
#include "il2cpp/Api.h"
namespace bl::csharp {
using runtime::NativeRecipeRequest;
void bindNativeRecipeAssembly(const std::string& mod,const std::string& assembly);
bool prepareNativeRecipeCallbacks(int mask,std::string* error);
runtime::RecipeCallbackReadiness prepareNativeRecipeCallbackProvider(const NativeRecipeRequest& request,std::string* error);
void trackNativeRecipeCallbacks(const NativeRecipeRequest& request,Il2CppObject* recipe,int index);
}
