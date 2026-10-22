#pragma once
#include "content/backgrounds/SurfaceGeometry.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "il2cpp/NullableLayout.h"

namespace bl::runtime::surface {
// Cached engine drawing contract. The caller supplies the engine's open
// SpriteBatch; this service never changes Begin/End or blend/sampler state.
class SurfaceSpriteBatch {
    const MethodInfo* drawMethod=nullptr;
    const MethodInfo* skyMethod=nullptr;
    Il2CppClass *batchClass=nullptr,*textureClass=nullptr,*skyClass=nullptr;
    std::array<size_t,2> pointOffsets{};
    std::array<size_t,4> rectangleOffsets{};
    csharp::NativeColorLayout colors;
    il2cpp::NullableLayout rectangle;
public:
    bool bind(Il2CppClass* batch,Il2CppClass* sky,std::string* error);
    bool draw(Il2CppObject* batch,Il2CppObject* sky,Il2CppObject* texture,const DrawPlan& plan,Color color,std::string* error,
              bool(*isCurrent)(void*)=nullptr,void* context=nullptr) const;
};
}
