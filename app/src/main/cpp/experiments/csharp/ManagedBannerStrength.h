#pragma once

#include "experiments/csharp/ManagedBannerRegistry.h"
#include "content/items/ModItems.h"
#include "content/npcs/ModNpcs.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::banners {

inline bool strengthItemSlotAllowed(int item, uintptr_t length) {
    return item >= runtime::kVanillaItemCount &&
           item < 262144 && static_cast<uintptr_t>(item) < length;
}

// Preparacao explicita na thread do jogo. Nao deve ser chamada antes de
// todos os outros contratos e hooks do BannerLoader estarem prontos.
inline bool enableManagedBannerStrengthOnMainThread(std::string* error) {
    if (!error) return false;
    auto snapshot = registry(runtime::kVanillaNpcCount).snapshot();
    if (snapshot->itemToBanner.empty()) return true;
    auto& api = il2cpp::api();
    if (!api.class_get_element_class || !api.class_is_valuetype ||
        !api.class_value_size || !api.object_get_class ||
        !api.field_static_get_value || !api.field_get_flags)
        return false;
    auto* cls = il2cpp::findClass({"Terraria.ID","ItemID",{}});
    auto* sets = cls ? il2cpp::findNested(cls,"Sets") : nullptr;
    auto* field = sets ? il2cpp::findField(sets,"BannerStrength") : nullptr;
    if (!field || (api.field_get_flags(field)&0x10u)==0) {
        *error = "ItemID.Sets.BannerStrength indisponivel";
        return false;
    }
    Il2CppArray* array=nullptr;
    api.field_static_get_value(field,&array);
    if (!array || array->length > 262144) {
        *error = "ItemID.Sets.BannerStrength tamanho invalido";
        return false;
    }
    auto* element = api.class_get_element_class(
        api.object_get_class(reinterpret_cast<Il2CppObject*>(array)));
    FieldInfo* enabled=nullptr;
    int offset=-1;
    if (!il2cpp::nativeFieldContract(element,"Enabled","System.Boolean",
                                     false,1,&enabled,&offset,error))
        return false;
    const bool valueType = api.class_is_valuetype(element);
    int stride=sizeof(void*);
    if (valueType) {
        uint32_t alignment=0;
        stride=api.class_value_size(element,&alignment);
        offset-=static_cast<int>(sizeof(Il2CppObject));
    }
    if (stride<=0 || stride>64 || offset<0 ||
        static_cast<int64_t>(offset)+1>stride) {
        *error = "BannerStrength.Enabled: tamanho/offset invalido";
        return false;
    }

    std::vector<uint8_t*> staging;
    staging.reserve(snapshot->itemToBanner.size());
    for (const auto& [item,banner] : snapshot->itemToBanner) {
        if (!strengthItemSlotAllowed(item,array->length)) {
            *error = "BannerStrength: tentativa de modificar item vanilla ou fora do limite";
            return false;
        }
        uint8_t* dest=nullptr;
        if (valueType) {
            dest=static_cast<uint8_t*>(arrayData(array))+
                static_cast<size_t>(item)*stride+offset;
        } else {
            auto* objects=static_cast<Il2CppObject**>(arrayData(array));
            auto* obj=objects[item];
            if (!obj || api.object_get_class(obj)!=element) {
                *error = "BannerStrength: instancia nao confiavel";
                return false;
            }
            dest=reinterpret_cast<uint8_t*>(obj)+offset;
        }
        staging.push_back(dest);
    }
    // Nenhum byte e alterado antes do preflight de TODOS os itens.
    for (uint8_t* dest : staging) *dest=1;
    error->clear();
    return true;
}

} // namespace bl::csharp::banners
