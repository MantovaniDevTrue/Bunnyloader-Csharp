#pragma once

#include "il2cpp/Api.h"
#include "experiments/csharp/ManagedBannerAnchorPolicy.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/ConstantField.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::banners {

inline bool configureNativeBannerAnchors(Il2CppClass* tileClass,
                                         Il2CppClass* dataClass,
                                         Il2CppObject* base,
                                         Il2CppObject* alternate,
                                         std::string* error) {
    if (!error || !tileClass || !dataClass || !base || !alternate) return false;
    auto& api=il2cpp::api();
    if (!api.class_is_valuetype || !api.class_instance_size ||
        !api.class_value_size || !api.object_new || !api.object_get_class ||
        !api.runtime_invoke || !api.field_get_type || !api.field_get_flags ||
        !api.class_from_il2cpp_type || !api.field_static_get_value ||
        !api.method_get_param_count || !api.method_get_param ||
        !api.class_get_name || !api.class_get_namespace)
        return false;

    auto* anchor=il2cpp::findClass({"Terraria.DataStructures","AnchorData",{}});
    auto* anchorType=il2cpp::findClass({"Terraria.Enums","AnchorType",{}});
    if (!anchor || !anchorType || !api.class_is_valuetype(anchor) ||
        !api.class_is_valuetype(anchorType)) {
        *error="BannerAnchor: AnchorData/AnchorType IL2CPP ausente";
        return false;
    }
    uint32_t alignment=0;
    const int anchorSize=api.class_value_size(anchor,&alignment);
    if (anchorSize<12 || anchorSize>64) {
        *error="BannerAnchor: tamanho de AnchorData incompatível";
        return false;
    }
    const MethodInfo* ctor=api.class_get_method_from_name(anchor,".ctor",3);
    const MethodInfo* top=api.class_get_method_from_name(tileClass,"set_AnchorTop",1);
    const MethodInfo* copy=api.class_get_method_from_name(tileClass,"CopyFrom",1);
    const MethodInfo* addAlternate=api.class_get_method_from_name(dataClass,"addAlternate",1);
    const MethodInfo* draw=api.class_get_method_from_name(tileClass,"set_DrawYOffset",1);
    if (!ctor || !top || !copy || !addAlternate || !draw ||
        api.method_get_param_count(ctor)!=3 ||
        api.class_from_il2cpp_type(api.method_get_param(ctor,0))!=anchorType ||
        api.class_from_il2cpp_type(api.method_get_param(ctor,1))!=
            il2cpp::findClass({"System","Int32",{}}) ||
        api.class_from_il2cpp_type(api.method_get_param(ctor,2))!=
            il2cpp::findClass({"System","Int32",{}}) ||
        api.class_from_il2cpp_type(api.method_get_param(top,0))!=anchor) {
        *error="BannerAnchor: assinatura nativa de AnchorData/TileObjectData diverge";
        return false;
    }
    if (api.object_get_class(base)!=tileClass ||
        api.object_get_class(alternate)!=tileClass) {
        *error="BannerAnchor: TileObjectData.newTile/newAlternate nao sao validos";
        return false;
    }

    const std::array<const char*,6> names={
        "SolidTile","SolidSide","SolidBottom","PlanterBox",
        "PlatformNonHammered","AllFlatHeight"
    };
    std::array<int32_t,6> kinds{};
    for (size_t i=0;i<names.size();++i) {
        auto* field=il2cpp::findField(anchorType,names[i]);
        if (!field || !il2cpp::readInt32Constant(field,anchorType,&kinds[i])) {
            *error=std::string("BannerAnchor: AnchorType ausente ")+names[i];
            return false;
        }
        if (!kinds[i]) {
            *error=std::string("BannerAnchor: AnchorType invalido ")+names[i];
            return false;
        }
    }
    if (!bannerAnchorMaskValid(kinds)) {
        *error="BannerAnchor: mascara AnchorType incompatível";
        return false;
    }
    const int32_t solid=kinds[0]|kinds[1]|kinds[2]|kinds[3];
    const int32_t platforms=kinds[4]|kinds[5];

    auto makeAnchor=[&](int32_t kind)->Il2CppObject* {
        auto* boxed=api.object_new(anchor);
        if (!boxed || api.object_get_class(boxed)!=anchor) return nullptr;
        int32_t width=kBannerAnchorPlan.coordinateWidth;
        int32_t first=kBannerAnchorPlan.anchorCheckStart;
        void* args[3]={&kind,&width,&first};
        Il2CppObject* exc=nullptr;
        api.runtime_invoke(ctor,boxed,args,&exc);
        return exc?nullptr:boxed;
    };

    // Cria ambos os AnchorData antes de publicar o alternate.
    auto* solidData=makeAnchor(solid);
    auto* platformData=makeAnchor(platforms);
    if (!solidData || !platformData) {
        *error="BannerAnchor: falha ao construir AnchorData";
        return false;
    }
    auto installAnchor=[&](Il2CppObject* dest,Il2CppObject* boxed)->bool {
        void* args[1]={reinterpret_cast<char*>(boxed)+sizeof(Il2CppObject)};
        Il2CppObject* exc=nullptr;
        api.runtime_invoke(top,dest,args,&exc);
        return !exc;
    };
    if (!installAnchor(base,solidData)) {
        *error="BannerAnchor: falha no AnchorTop solido";
        return false;
    }
    void* copyArgs[1]={base};
    Il2CppObject* exc=nullptr;
    api.runtime_invoke(copy,alternate,copyArgs,&exc);
    if (exc || !installAnchor(alternate,platformData)) {
        *error="BannerAnchor: CopyFrom/AnchorTop alternativo falhou";
        return false;
    }
    int32_t offset=kBannerAnchorPlan.platformDrawYOffset;
    void* shiftArgs[1]={&offset};
    exc=nullptr;
    api.runtime_invoke(draw,alternate,shiftArgs,&exc);
    if (exc) {
        *error="BannerAnchor: DrawYOffset alternativo divergente";
        return false;
    }
    // O alternate copia do tile configurado (altura 3, correcao -2).
    // O CopyFrom anterior a AnchorTop alternativo e feito pelo chamador.
    int32_t alternateStyle=kBannerAnchorPlan.platformAlternateIndex;
    void* addArgs[1]={&alternateStyle};
    api.runtime_invoke(addAlternate,nullptr,addArgs,&exc);
    if (exc) {
        *error="BannerAnchor: addAlternate(0) falhou";
        return false;
    }
    return true;
}

} // namespace bl::csharp::banners
