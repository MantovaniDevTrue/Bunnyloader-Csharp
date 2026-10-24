#pragma once
#include "il2cpp/Api.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bl::csharp::table_publication {
// Keep pending arrays and assets alive until the native static tables own them.
template<class Api> class Roots {
    Api& api;
    std::vector<uint32_t> handles;
    bool uncertain = false;
public:
    explicit Roots(Api& value) : api(value) {}
    Roots(const Roots&) = delete;
    Roots& operator=(const Roots&) = delete;
    ~Roots() { if (!uncertain) for (auto h : handles) api.gchandle_free(h); }
    bool add(Il2CppObject* object) {
        if (!object || handles.size() >= 2048 || !api.gchandle_new ||
            !api.gchandle_get_target || !api.gchandle_free) return false;
        const auto h = api.gchandle_new(object, true);
        if (!h) return false;
        if (api.gchandle_get_target(h) != object) { api.gchandle_free(h); return false; }
        handles.push_back(h);
        return true;
    }
    void retainOnUncertainRollback() { uncertain = true; }
};

template<class Api> bool publish(Api& api, FieldInfo* const* fields,
    Il2CppArray* const* original, Il2CppArray* const* pending,
    size_t count, Roots<Api>& roots) {
    if (!fields || !original || !pending || count == 0 || count > 6 ||
        !api.field_static_get_value || !api.field_static_set_value) return false;
    // Reject a competing publisher before writing anything.
    for (size_t i = 0; i < count; ++i) {
        Il2CppArray* current = nullptr;
        api.field_static_get_value(fields[i], &current);
        if (!original[i] || !pending[i] || current != original[i]) return false;
    }
    for (size_t i = 0; i < count; ++i) {
        // IL2CPP static reference writes take the object, not &object.
        api.field_static_set_value(fields[i], pending[i]);
        Il2CppArray* current = nullptr;
        api.field_static_get_value(fields[i], &current);
        if (current == pending[i]) continue;
        bool restored = true;
        for (size_t j = 0; j <= i; ++j) {
            api.field_static_set_value(fields[j], original[j]);
            Il2CppArray* prior = nullptr;
            api.field_static_get_value(fields[j], &prior);
            restored &= prior == original[j];
        }
        if (!restored) roots.retainOnUncertainRollback();
        return false;
    }
    return true;
}
}
