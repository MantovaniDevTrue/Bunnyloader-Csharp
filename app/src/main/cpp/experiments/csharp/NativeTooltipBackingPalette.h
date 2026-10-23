#pragma once
#include <array>
#include <string_view>

namespace bl::csharp {

enum class NativeTooltipBackingColorSource {
    TooltipBacking,
    Settings
};

struct NativeTooltipBackingPalette {
    NativeTooltipBackingColorSource source=NativeTooltipBackingColorSource::TooltipBacking;
    std::string_view name="BackingColor";
    std::string_view border="BorderColor";
    std::string_view backingTexture="Backing";
    std::string_view borderTexture="Border";
};

inline NativeTooltipBackingPalette nativeTooltipBackingPalette(int context,int loadout) {
    NativeTooltipBackingPalette value;
    value.source=NativeTooltipBackingColorSource::Settings;
    switch(context) {
    case 29:value.name="DuplicationTooltipBacking";break;
    case 32:value.name="VoidItemTooltipBacking";break;
    case 35:value.name="BannerTooltipBacking";break;
    case 15:value.name="ShopTooltipBacking";break;
    case 3:value.name="ChestTooltipBacking";break;
    case 44:value.name="ResearchTooltipBacking";break;
    case 4:value.name="BankTooltipBacking";break;
    case 5:value.name="PrefixTooltipBacking";break;
    case 47:value.name="CraftingStationTooltipBacking";break;
    case 8:case 9:case 10:case 11:case 12:
        if(loadout>=0&&loadout<=2) {
            constexpr std::array<std::string_view,3> colors{
                "Equip0TooltipBacking","Equip1TooltipBacking","Equip2TooltipBacking"};
            value.name=colors[static_cast<size_t>(loadout)];
            break;
        }
        [[fallthrough]];
    default:
        if(context==16||context==17||context==18||
           context==19||context==20||context==33) {
            value.name="Equip0TooltipBacking";break;
        }
        value.source=NativeTooltipBackingColorSource::TooltipBacking;
        value.name="BackingColor";
        break;
    }
    return value;
}

}
