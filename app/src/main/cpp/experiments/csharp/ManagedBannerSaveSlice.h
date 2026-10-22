#pragma once

#include "content/common/TypeTables.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/ConstantField.h"
#include "il2cpp/Resolver.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::banners {

inline bool vanillaBannerSaveRange(int vanilla, uintptr_t kills,
                                   uintptr_t claimable) {
    return vanilla > 0 && vanilla <= 4096 &&
           kills >= static_cast<uintptr_t>(vanilla) &&
           claimable >= static_cast<uintptr_t>(vanilla) &&
           kills <= 262144 && claimable <= 262144;
}

struct BannerSaveFields {
    FieldInfo* kills = nullptr;
    FieldInfo* claimable = nullptr;
    int vanilla = 0;
};

inline bool resolveVanillaBannerSaveFields(BannerSaveFields* fields,
                                           std::string* error) {
    if (!fields || !error) return false;
    auto& api=il2cpp::api();
    if (!api.field_static_get_value || !api.field_static_set_value ||
        !api.class_get_element_class || !api.object_get_class ||
        !api.gchandle_new || !api.gchandle_free || !api.array_new) {
        *error="BannerSave: exports IL2CPP indisponiveis";
        return false;
    }
    auto* banner=il2cpp::findClass({"Terraria.GameContent","BannerSystem",{}});
    BannerSaveFields next{};
    FieldInfo* maximum=nullptr;
    if (!il2cpp::nativeFieldContract(banner,"killCount","System.Int32[]",
         true,sizeof(void*),&next.kills,nullptr,error) ||
        !il2cpp::nativeFieldContract(banner,"claimableBanners","System.Int32[]",
         true,sizeof(void*),&next.claimable,nullptr,error) ||
        !il2cpp::nativeFieldContract(banner,"MaxBannerTypes","System.Int32",
         true,sizeof(int32_t),&maximum,nullptr,error))
        return false;
    if (!il2cpp::readInt32Constant(maximum,
                                   il2cpp::findClass({"System","Int32",{}}),
                                   &next.vanilla)) {
        *error="BannerSave: MaxBannerTypes nao pode ser lido";
        return false;
    }
    if (next.vanilla <= 0 || next.vanilla > 4096) {
        *error="BannerSave: MaxBannerTypes invalido ou constante nao resolvida";
        return false;
    }
    *fields=next;
    return true;
}

// A chamada ao Save original deve ocorrer dentro deste escopo e fora do
// NetBannersModule.WriteFullState. Diferentemente do pacote de rede, o
// arquivo .wld vanilla so pode receber os contadores vanilla.
class ScopedVanillaBannerSave final {
public:
    explicit ScopedVanillaBannerSave(const BannerSaveFields& fields)
      : fields_(fields) {}

    ScopedVanillaBannerSave(const ScopedVanillaBannerSave&)=delete;
    ScopedVanillaBannerSave& operator=(const ScopedVanillaBannerSave&)=delete;

    bool begin(std::string* error) {
        if (!error || begun_) return false;
        auto& api=il2cpp::api();
        if (!fields_.kills || !fields_.claimable) return false;
        std::array<FieldInfo*,2> names{fields_.kills,fields_.claimable};
        for (size_t i=0;i<2;++i) {
            api.field_static_get_value(names[i],&original_[i]);
            if (!il2cpp::nativeArrayContract(original_[i],"Int32",4,262144)) {
                *error="BannerSave: array de contador invalido";
                return false;
            }
        }
        if (!vanillaBannerSaveRange(fields_.vanilla,original_[0]->length,
                                    original_[1]->length)) {
            *error="BannerSave: limite vanilla diverge dos arrays";
            return false;
        }
        if (original_[0]->length == static_cast<uintptr_t>(fields_.vanilla) &&
            original_[1]->length == static_cast<uintptr_t>(fields_.vanilla)) {
            begun_=true;
            return true;
        }
        for (size_t i=0;i<2;++i) {
            roots_[i]=api.gchandle_new(
                reinterpret_cast<Il2CppObject*>(original_[i]),false);
            if (!roots_[i]) {
                *error="BannerSave: falha ao fixar array original no GC";
                return false;
            }
            Il2CppClass* element=api.class_get_element_class(
                api.object_get_class(reinterpret_cast<Il2CppObject*>(original_[i])));
            replacement_[i]=api.array_new(element,fields_.vanilla);
            if (!il2cpp::nativeArrayContract(replacement_[i],"Int32",4,4096)) {
                *error="BannerSave: copia vanilla invalida";
                return false;
            }
            std::memcpy(arrayData(replacement_[i]),arrayData(original_[i]),
                        static_cast<size_t>(fields_.vanilla)*4);
            roots_[i+2]=api.gchandle_new(
                reinterpret_cast<Il2CppObject*>(replacement_[i]),false);
            if (!roots_[i+2]) {
                *error="BannerSave: copia nao protegida pelo GC";
                return false;
            }
        }
        api.field_static_set_value(fields_.kills,replacement_[0]);
        swapped_=1;
        api.field_static_set_value(fields_.claimable,replacement_[1]);
        swapped_=2;
        begun_=true;
        return true;
    }

    ~ScopedVanillaBannerSave() {
        auto& api=il2cpp::api();
        if (swapped_ >= 1 && api.field_static_set_value)
            api.field_static_set_value(fields_.kills,original_[0]);
        if (swapped_ >= 2 && api.field_static_set_value)
            api.field_static_set_value(fields_.claimable,original_[1]);
        if (api.gchandle_free)
            for (uint32_t root : roots_) if (root) api.gchandle_free(root);
    }

private:
    BannerSaveFields fields_;
    std::array<Il2CppArray*,2> original_{};
    std::array<Il2CppArray*,2> replacement_{};
    std::array<uint32_t,4> roots_{};
    int swapped_=0;
    bool begun_=false;
};

} // namespace bl::csharp::banners
