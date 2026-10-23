#include "experiments/csharp/NativeTooltipBackingPalette.h"
#include <cassert>
#include <iostream>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

using namespace bl::csharp;

int main() {
    const std::pair<int,std::string_view> special[]={
        {29,"DuplicationTooltipBacking"},{32,"VoidItemTooltipBacking"},
        {35,"BannerTooltipBacking"},{15,"ShopTooltipBacking"},
        {3,"ChestTooltipBacking"},{44,"ResearchTooltipBacking"},
        {4,"BankTooltipBacking"},{5,"PrefixTooltipBacking"},
        {47,"CraftingStationTooltipBacking"}};
    for(const auto& [context,name]:special)
        for(int loadout:{-1,0,1,2,3}) {
            const auto result=nativeTooltipBackingPalette(context,loadout);
            assert(result.source==NativeTooltipBackingColorSource::Settings);
            assert(result.name==name);
            assert(result.border=="BorderColor"&&result.backingTexture=="Backing"&&result.borderTexture=="Border");
        }
    const std::string_view loadouts[]{"Equip0TooltipBacking","Equip1TooltipBacking","Equip2TooltipBacking"};
    for(int context:{8,9,10,11,12}) {
        for(int loadout=0;loadout<3;++loadout) {
            const auto result=nativeTooltipBackingPalette(context,loadout);
            assert(result.source==NativeTooltipBackingColorSource::Settings);
            assert(result.name==loadouts[loadout]);
        }
        for(int loadout:{-2,3,8}) {
            const auto result=nativeTooltipBackingPalette(context,loadout);
            assert(result.source==NativeTooltipBackingColorSource::TooltipBacking);
            assert(result.name=="BackingColor");
        }
    }
    for(int context:{16,17,18,19,20,33})
        for(int loadout:{-2,0,1,2,3}) {
            const auto result=nativeTooltipBackingPalette(context,loadout);
            assert(result.source==NativeTooltipBackingColorSource::Settings);
            assert(result.name=="Equip0TooltipBacking");
        }
    std::set<int> specialized{29,32,35,15,3,44,4,5,47,8,9,10,11,12,16,17,18,19,20,33};
    for(int context=-5;context<80;++context) {
        if(specialized.contains(context))continue;
        for(int loadout:{-1,0,1,2,3}) {
            const auto result=nativeTooltipBackingPalette(context,loadout);
            assert(result.source==NativeTooltipBackingColorSource::TooltipBacking);
            assert(result.name=="BackingColor");
        }
    }
    std::cout<<"CSHARP_TOOLTIP_BACKING_PALETTE=PASS contexts=85\n";
}
