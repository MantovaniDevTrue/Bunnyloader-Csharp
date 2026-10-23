#pragma once
#include "content/recipes/NativeRecipes.h"
#include "content/recipes/RecipeCallbackProvider.h"
#include "il2cpp/Api.h"
#include "experiments/csharp/ManagedAssemblyBindings.h"
namespace bl::csharp {
using runtime::NativeRecipeRequest;
void bindNativeRecipeAssembly(const std::string& mod,const std::string& assembly);
void unbindNativeRecipeAssembly(const std::string& mod);
inline ManagedAssemblyBindings& nativeRecipeAssemblyBindings() {
    static ManagedAssemblyBindings bindings;
    return bindings;
}
inline bool isNativeRecipeAssemblyCurrent(const std::string& mod,const std::string& assembly) {
    return nativeRecipeAssemblyBindings().isCurrent(mod,assembly);
}
bool hasManagedAnglerQuestCallbacks(const std::string& mod);
bool invokeManagedAnglerQuestCallback(const std::string& mod,const std::string& owner,
                                      int kind,int first,int second,int itemType,bool* allowed,int playerIndex=-1);
bool prepareNativeRecipeCallbacks(int mask,std::string* error);
runtime::RecipeCallbackReadiness prepareNativeRecipeCallbackProvider(const NativeRecipeRequest& request,std::string* error);
void trackNativeRecipeCallbacks(const NativeRecipeRequest& request,Il2CppObject* recipe,int index);
}
