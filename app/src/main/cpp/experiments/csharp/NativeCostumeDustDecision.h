#pragma once
#include <cstdint>

namespace bl::csharp::costume_dust {

inline bool visible(bool forceVanity,bool hasPower,bool hideVanity) {
    return forceVanity||(hasPower&&!hideVanity);
}

inline bool sample(uint32_t& random,int playerIndex) {
    if(playerIndex<0||playerIndex>=256)return false;
    if(random==0)
        random=0x9e3779b9u^((static_cast<uint32_t>(playerIndex)+1u)*0x85ebca6bu);
    random^=random<<13;
    random^=random>>17;
    random^=random<<5;
    return random%20u==0u;
}

inline int dustId(bool wet,int normal,int alternate) {
    return wet?alternate:normal;
}

}
