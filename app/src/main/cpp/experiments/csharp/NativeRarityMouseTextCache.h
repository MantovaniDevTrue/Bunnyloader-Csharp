#pragma once
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include <array>
#include <limits>

namespace bl::csharp {
// MouseTextInner recebe o cache por valor. No AAPCS64, esta estrutura mista
// de 48 bytes (com referências) passa indiretamente, por endereço do payload.
// Esse endereço NÃO tem cabeçalho Il2CppObject. Outros ABIs não são presumidos.
struct NativeRarityMouseTextCache {
    int rare = -1, text = -1, buff = -1, diff = -1, diffWidth = 0;
#if defined(__aarch64__) && __SIZEOF_POINTER__ == 8
    static constexpr bool indirectValueAbi = true;
#else
    static constexpr bool indirectValueAbi = false;
#endif

    bool bind(Il2CppClass* cache, Il2CppClass* integer, Il2CppClass* byte,
              Il2CppClass* boolean, Il2CppClass* string, std::string* error,
              bool supportsIndirectValue = indirectValueAbi) {
        using namespace bl::il2cpp;
        *this = {};
        auto& a = api();
        auto fail = [&](const char* why) { *error = why; return false; };
        if (!cache || !integer || !byte || !boolean || !string ||
            !a.class_is_valuetype || !a.field_get_type || !a.field_get_flags ||
            !a.field_get_offset || !a.class_from_il2cpp_type)
            return fail("Reason=MouseTextCacheMetadataUnavailable");
        const bool value = a.class_is_valuetype(cache);
        ValueLayout layout;
        if (value && (!supportsIndirectValue || sizeof(void*) != 8))
            return fail("Reason=MouseTextCacheValueAbiUnsupported");
        if (value && !resolveValueLayout(cache, &layout, error)) return false;
        if (value && layout.size != 48)
            return fail("Reason=MouseTextCacheValueSizeMismatch");
        auto field = [&](const char* name, Il2CppClass* expected, size_t width,
                         size_t expectedOffset, int* output) {
            auto* f = findField(cache, name);
            if (!f || (a.field_get_flags(f) & 0x10u) ||
                a.class_from_il2cpp_type(a.field_get_type(f)) != expected) return false;
            size_t offset = a.field_get_offset(f);
            if (value) {
                if (!layout.offset(offset, width, &offset) || offset != expectedOffset)
                    return false;
            } else if (offset < sizeof(Il2CppObject)) return false;
            if (offset > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
            *output = static_cast<int>(offset);
            return true;
        };
        NativeRarityMouseTextCache next;
        if (!field("rare", integer, 4, 16, &next.rare) ||
            !field("cursorText", string, sizeof(void*), 8, &next.text) ||
            !field("buffTooltip", string, sizeof(void*), 40, &next.buff))
            return fail("Reason=MouseTextCacheFieldsUnavailable");
        next.diffWidth = 1;
        if (!field("diff", byte, 1, 20, &next.diff)) {
            // A classe usada por versões anteriores pode guardar diff em Int32.
            if (value || !field("diff", integer, 4, 20, &next.diff))
                return fail("Reason=MouseTextDiffAbiUnavailable");
            next.diffWidth = 4;
        }
        if (value) {
            int ignored = 0;
            // Validar a estrutura inteira evita aceitar um HFA ou outra assinatura
            // apenas porque os quatro campos utilizados têm nomes conhecidos.
            if (!field("noOverride", boolean, 1, 0, &ignored) ||
                !field("isValid", boolean, 1, 1, &ignored))
                return fail("Reason=MouseTextCacheValueShapeMismatch");
            const std::array<const char*, 4> names{"X", "Y", "hackedScreenWidth", "hackedScreenHeight"};
            for (size_t i = 0; i < names.size(); ++i)
                if (!field(names[i], integer, 4, 24 + 4 * i, &ignored))
                    return fail("Reason=MouseTextCacheValueShapeMismatch");
        }
        *this = next;
        return true;
    }
};
}
