#include "experiments/csharp/BasicItemDefaults.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace bl::csharp;
    BasicItemEntry item;item.name="ContractGroupedBench";item.values[BasicMaxStack]=9999;
    std::string error;
    assert(parseBasicRecipes("1,mBench,gWood:10,gIronBar:2,mMaterial:3",&item,&error));
    assert(item.recipes.size()==1&&item.recipes[0].stationKey=="Bench");
    const auto& values=item.recipes[0].ingredients;
    assert(values.size()==3&&values[0].key=="gWood"&&values[0].count==10&&values[0].type==-1);
    assert(values[1].key=="gIronBar"&&values[1].count==2);
    for(const auto& bad:{"g:1","g../Wood:1","gWood/Other:1","g1Wood:1","gWood:0","gWood:10000","gWood:1:2","gWood;:1"}) {
        BasicItemEntry rejected;rejected.name="ContractGroupedBench";
        assert(!parseBasicRecipes(std::string("1,-1,")+bad,&rejected,&error));
        assert(error.find("ContractGroupedBench")!=std::string::npos);
    }
    BasicItemEntry old;old.name="LegacyRecipe";
    assert(parseBasicRecipes("1,18,v2:1,mother/Material:2",&old,&error));
    assert(old.recipes[0].ingredients[0].type==2&&old.recipes[0].ingredients[1].key=="mother/Material");
    std::cout<<"RECIPE_GROUP_WIRE_AND_LEGACY_COMPATIBILITY_PASS\n";
}
