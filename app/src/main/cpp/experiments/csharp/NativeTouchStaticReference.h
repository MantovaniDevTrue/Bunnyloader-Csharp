#pragma once

namespace bl::csharp::touch {
template<typename Field, typename Array>
bool publishNativeTouchStaticReference(
    Field* field, Array* array,
    void (*set)(Field*, void*),
    void (*get)(Field*, void*)) {
    if (!field || !array || !set || !get) return false;
    set(field, array);
    Array* actual = nullptr;
    get(field, &actual);
    return actual == array;
}
}
