#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
namespace bl::csharp {
struct NativeVisualRendererStatus {bool ready=false;std::string error;size_t assets=0;uint64_t drawn=0,rejected=0;};
void requestNativeVisualRenderer(const std::string& texture = "");
size_t pendingNativeVisualRenderer();
void pumpNativeVisualRendererOnGameThread();
void resetNativeVisualRenderer();
NativeVisualRendererStatus nativeVisualRendererStatus();
}
