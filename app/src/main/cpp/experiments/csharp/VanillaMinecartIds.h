#pragma once
namespace bl::csharp::vanilla_minecarts {
inline constexpr int meowmere=33;
inline constexpr int fart=51;
constexpr int resolveId(int expected,int metadataValue) {
    return metadataValue==-1||metadataValue==expected?expected:-1;
}
}
