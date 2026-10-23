#pragma once
#include "experiments/csharp/NativeTooltipPanelSettings.h"
#include "experiments/csharp/NativeTooltipPanelColors.h"
#include "experiments/csharp/NativeTooltipPanelBacking.h"

namespace bl::csharp {

struct NativeTooltipPanelInspection {
    NativeTooltipPanelResources resources;
    std::array<uint8_t,4> fillRgba{};
    std::array<uint8_t,4> borderRgba{};
};

inline bool inspectNativeTooltipPanel(Il2CppClass* settings,Il2CppClass* panel,
                                     int tooltipContext,int loadout,
                                     NativeTooltipPanelInspection* out,
                                     std::string* error) {
    if(!out||!error||!settings||!panel)return false;
    NativeTooltipPanelResourcePath source;
    if(!source.bind(settings,error))return false;
    NativeTooltipPanelColors colors;
    if(!colors.bind(settings,source.textures.backing.type,tooltipContext,loadout,error))
        return false;
    NativeTooltipPanelBacking backing;
    if(!backing.bind(panel,error))return false;
    if(backing.textureType!=source.textures.fillTexture.type||
       backing.textureType!=source.textures.borderTexture.type) {
        *error="Reason=NativeTooltipPanelBackingTextureAbiMismatch";
        return false;
    }
    NativeTooltipPanelInspection stage;
    if(!source.read(&stage.resources)) {
        *error="Reason=NativeTooltipPanelInstanceOrTextureUnavailable";
        return false;
    }
    if(!colors.read(stage.resources.settings,stage.resources.tooltipBacking,
                    &stage.fillRgba,&stage.borderRgba)) {
        *error="Reason=NativeTooltipPanelColorReadUnavailable";
        return false;
    }
    *out=stage;
    return true;
}

}
