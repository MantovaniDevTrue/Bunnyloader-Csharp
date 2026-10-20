#pragma once
#include <map>

namespace bl::csharp::tree_soil {
inline constexpr int customTreeType=13;
inline bool supportedGround(int ground,int sapling) {
    return ground>=754&&ground<65536&&sapling>=754&&sapling<65536;
}
inline bool canRegister(const std::map<int,int>& entries,int ground,int sapling) {
    if(!supportedGround(ground,sapling))return false;
    const auto it=entries.find(ground);
    return it==entries.end()||it->second==sapling;
}
inline bool owns(const std::map<int,int>& entries,int ground) {
    return entries.find(ground)!=entries.end();
}
}
