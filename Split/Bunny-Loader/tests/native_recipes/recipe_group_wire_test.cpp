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
    BasicItemEntry custom;custom.name="CustomBench";
    assert(parseBasicRecipes("1,-1,hMaterials~416E79206D6174657269616C~v9+mExampleItem:3@7",&custom,&error));
    assert(custom.recipes[0].callbacks==7);
    const auto& group=custom.recipes[0].ingredients[0];
    assert(group.customGroup=="Materials"&&group.groupDescription=="Any material"&&group.groupMembers.size()==2);
    for(const auto& bad:{"hMaterials~GG~v9:1","hMaterials~00~v9:1","hMaterials~41~:1","hbad/name~41~v9:1","hMaterials~41~gWood:1"}) {
        BasicItemEntry rejected;rejected.name="BadCustomGroup";
        assert(!parseBasicRecipes(std::string("1,-1,")+bad,&rejected,&error));
    }
    for(const auto& suffix:{"@0","@8","@7@7","@-1"}) {
        BasicItemEntry rejected;rejected.name="BadPolicy";
        assert(!parseBasicRecipes(std::string("1,-1,v2:1")+suffix,&rejected,&error));
    }
    std::cout<<"RECIPE_GROUP_WIRE_AND_LEGACY_COMPATIBILITY_PASS\n";
}
