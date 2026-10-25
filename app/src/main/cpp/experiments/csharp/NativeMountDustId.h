#pragma once
namespace bl::csharp::mount_dust {
inline constexpr int gemDiamond=91;
inline constexpr int resolveGemDiamond(int nativeValue) {
    return nativeValue==-1||nativeValue==gemDiamond?gemDiamond:-1;
}
}
