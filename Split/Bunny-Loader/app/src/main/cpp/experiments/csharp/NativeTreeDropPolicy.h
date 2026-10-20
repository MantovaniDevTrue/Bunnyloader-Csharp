#pragma once

namespace bl::csharp::tree_drop {
inline bool valid(int wood,bool acorns) {
    (void)acorns;
    return wood==-1 || (wood>=6196&&wood<65536);
}
inline bool shouldAddTopAcorn(int secondary,int acornId,bool allowed,
                              int frameX,int frameY,int netMode,double chance) {
    return secondary==0&&acornId>0&&allowed&&netMode!=1&&
           netMode>=0&&netMode<=2&&frameX>=22&&frameY>=198&&
           chance>=0.0&&chance<0.5;
}
inline void apply(bool isWood,int acornId,int woodItem,bool allowAcorns,
                  int* primary,int* secondary) {
    if(!primary||!secondary)return;
    if(isWood&&woodItem>=6196)*primary=woodItem;
    if(!allowAcorns&&acornId>0&&*secondary==acornId)*secondary=0;
}
}
