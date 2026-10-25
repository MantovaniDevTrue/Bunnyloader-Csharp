#pragma once
namespace bl::csharp::tree_acorn {
inline constexpr int vanillaId=27;
constexpr int resolveId(int runtimeValue) {
    return runtimeValue==-1||runtimeValue==vanillaId?vanillaId:-1;
}
}
