#pragma once
#include <cstdint>

namespace bl::csharp::tree_plant {
inline constexpr int vanillaSapling=20;
inline bool valid(int ground,int sapling) {
    return ground>=754&&ground<65536&&sapling>=754&&sapling<65536;
}
inline bool rewrite(int incomingType,int groundType,int configuredGround,
                    int configuredSapling,int* type,int* style) {
    if(!type||!style||incomingType!=vanillaSapling||groundType!=configuredGround||
       !valid(configuredGround,configuredSapling))return false;
    *type=configuredSapling;
    *style=0;
    return true;
}
}
