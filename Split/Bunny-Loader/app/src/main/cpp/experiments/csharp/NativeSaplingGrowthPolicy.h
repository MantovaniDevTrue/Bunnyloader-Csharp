#pragma once
namespace bl::csharp::tile_growth {
inline bool valid(int shape,int chance) {
    return chance>=0&&chance<=1024&&(chance==0||shape==4);
}
inline bool hit(int chance,double sample) {
    return chance>=1&&chance<=1024&&sample>=0.0&&
           sample<1.0/static_cast<double>(chance);
}
}
