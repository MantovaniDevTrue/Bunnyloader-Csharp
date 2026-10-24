#pragma once
#include <string_view>

namespace bl::csharp::tile_sapling {
inline bool validInput(int x,int y,int addon) {
    return x>=0&&y>=0&&x<=100000&&y<=100000&&addon>=-64&&addon<=64;
}
inline bool parseReply(std::string_view response,bool* allow) {
    if(!allow)return false;
    if(response=="1"){*allow=true;return true;}
    if(response=="0"){*allow=false;return true;}
    return false;
}
}
