#pragma once
#include <cstddef>
namespace bl::csharp {
inline constexpr size_t kSourceMaxFiles=4096;
inline constexpr size_t kSourceMaxFileBytes=1024*1024;
inline constexpr size_t kSourceMaxTotalBytes=32*1024*1024;
inline constexpr size_t kSourceMaxTypes=8192;
inline constexpr size_t kSourceMaxCategory=4096;
inline constexpr size_t kSourceDefaultsBytes=8*1024*1024;
inline constexpr size_t kSourceMotionBytes=4*1024*1024;
inline constexpr size_t kSourceMaxAssemblyBytes=32*1024*1024;
inline constexpr int kSourceMaxDepth=24;
inline constexpr bool validSourceExecutedTypeCount(int count) {return count>0&&static_cast<size_t>(count)<=kSourceMaxTypes;}
}
