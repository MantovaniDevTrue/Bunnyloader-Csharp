#pragma once
#include <cstddef>
namespace bl::csharp {
inline constexpr size_t kSourceMaxFiles=512;
inline constexpr size_t kSourceMaxFileBytes=256*1024;
inline constexpr size_t kSourceMaxTotalBytes=16*1024*1024;
inline constexpr size_t kSourceMaxTypes=1024;
inline constexpr size_t kSourceMaxCategory=256;
inline constexpr size_t kSourceDefaultsBytes=2*1024*1024;
inline constexpr size_t kSourceMotionBytes=1024*1024;
inline constexpr bool validSourceExecutedTypeCount(int count) {return count>0&&static_cast<size_t>(count)<=kSourceMaxTypes;}
}
