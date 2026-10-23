#pragma once
#include "experiments/csharp/NativeTooltipPanelAlpha.h"
#include "experiments/csharp/NativeTooltipPanelBacking.h"
#include "experiments/csharp/NativeTooltipPanelInspection.h"

namespace bl::csharp {

struct NativeTooltipPanelRenderer {
    NativeTooltipPanelBacking backing;
    NativeTooltipPanelAlpha opacity;

    bool bind(Il2CppClass* panel,Il2CppClass* color,std::string* error) {
        *this={};
        if(!backing.bind(panel,error))return false;
        NativeTooltipPanelAlpha bound;
        if(!bound.bind(color,error))return false;
        if(bound.colorType!=bl::il2cpp::api().class_from_il2cpp_type(
                bl::il2cpp::api().method_get_param(backing.method,3))) {
            if(error)*error="Reason=NativeTooltipPanelRendererColorTypeMismatch";
            return false;
        }
        opacity=bound;
        return true;
    }

    bool draw(const NativeTooltipPanelResult& panel,
              const NativeTooltipPanelInspection& inspected,float alpha) const {
        if(!panel.opaque||!inspected.resources.fillTexture||
           !inspected.resources.borderTexture||!std::isfinite(alpha)||
           alpha<0.f||alpha>1.f)return false;
        std::array<uint8_t,4> fill{},stroke{};
        if(!opacity.apply(inspected.fillRgba,alpha,&fill)||
           !opacity.apply(inspected.borderRgba,alpha,&stroke))return false;
        auto drawOne=[&](Il2CppObject* texture,const std::array<uint8_t,4>& rgba) {
            return backing.draw(panel,texture,rgba[0],rgba[1],rgba[2],rgba[3]);
        };
        if(!drawOne(inspected.resources.fillTexture,fill))return false;
        return drawOne(inspected.resources.borderTexture,stroke);
    }
};

}
