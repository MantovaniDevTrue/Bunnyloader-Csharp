#pragma once
#include "experiments/csharp/BasicItemDefaults.h"

namespace bl::csharp {
struct SourceRecipeOwner {
    std::string name,kind,source;
    int type=-1;
    std::vector<BasicRecipe> recipes;
};
inline std::string recipeOwnerBase(const std::string& kind) {
    return kind=="WEAPON"?"BunnyWeapon":kind=="ACCESSORY"?"BunnyAccessory":kind=="POTION"?"BunnyBuffPotion":"";
}
inline bool parseSourceRecipeOwner(const std::vector<std::string>& fields,const std::string& mod,
                                  SourceRecipeOwner* out,std::string* error) {
    auto fail=[&](const std::string& reason,const std::string& expected,const std::string& actual) {
        *error="[CSharp][ERROR][RecipeLoader] Mod="+mod+" Class="+(fields.size()>1?fields[1]:"Missing")+
            " Stage=DecodeOwner Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;return false;
    };
    if(fields.size()!=4||fields[0]!="RECIPES")return fail("InvalidOwnerRow","RECIPES,Name,Kind,Definitions",std::to_string(fields.size()));
    if(fields[1].empty()||fields[1].size()>72)return fail("InvalidOwnerName","ClassName1..72",fields[1]);
    for(size_t i=0;i<fields[1].size();++i){unsigned char c=fields[1][i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||(i>0&&c>='0'&&c<='9')))
            return fail("InvalidOwnerName","AsciiCSharpClassName",fields[1]);
    }
    if(recipeOwnerBase(fields[2]).empty())return fail("UnsupportedOwnerKind","WEAPON/ACCESSORY/POTION",fields[2]);
    BasicItemEntry decoded;decoded.name=fields[1];
    // Wire bound only. NativeRecipes later checks the actual result's maxStack after SetDefaults.
    decoded.values[BasicMaxStack]=9999;
    std::string detail;
    if(!parseBasicRecipes(fields[3],&decoded,&detail)||decoded.recipes.empty())
        return fail("InvalidOwnerRecipes","1..16ValidRecipeDescriptors",detail.empty()?fields[3]:detail);
    *out={fields[1],fields[2],"",-1,std::move(decoded.recipes)};return true;
}
}
