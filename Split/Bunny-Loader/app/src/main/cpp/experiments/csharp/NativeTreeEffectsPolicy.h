#pragma once
namespace bl::csharp::tree_effect {
inline bool valid(int leaf,int dust) {
    return leaf>=-1&&leaf<32768&&dust>=0&&dust<=1999;
}
inline bool acceptLeaf(int groundType,bool registered,int leaf) {
    return registered&&groundType>=754&&groundType<65536&&leaf>=-1&&leaf<32768;
}
inline bool acceptDust(int type,int dust) {
    return type==5&&dust>=0&&dust<=1999;
}
}
