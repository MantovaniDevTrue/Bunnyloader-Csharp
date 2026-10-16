#pragma once

#include "experiments/csharp/ManagedVisualDrain.h"
#include <cstddef>
#include <string>

namespace bl::csharp {

struct ManagedVisualMonoApi {
    using Invoke = void* (*)(void*, void*, void**, void**);
    using Utf8 = char* (*)(void*);
    using Free = void (*)(void*);
    Invoke invoke = nullptr;
    Utf8 utf8 = nullptr;
    Free freeString = nullptr;
    void* takeWireMethod = nullptr;
};

class ManagedVisualMonoPump {
public:
    static ManagedVisualDrainStats poll(ManagedVisualQueue& queue,
                                         const ManagedVisualMonoApi& api,
                                         size_t budget = ManagedVisualDrain::MaxPerUpdate) {
        if (!api.invoke || !api.utf8 || !api.freeString || !api.takeWireMethod)
            return {};
        return ManagedVisualDrain::poll(queue, [&](std::string& wire) {
            void* exception = nullptr;
            void* result = api.invoke(api.takeWireMethod, nullptr, nullptr, &exception);
            if (exception || !result) return false;
            char* chars = api.utf8(result);
            if (!chars) return false;
            size_t length = 0;
            while (length <= ManagedVisualWire::MaxBytes && chars[length])
                ++length;
            if (length <= ManagedVisualWire::MaxBytes)
                wire.assign(chars, length);
            api.freeString(chars);
            return !wire.empty();
        }, budget);
    }
};

}
