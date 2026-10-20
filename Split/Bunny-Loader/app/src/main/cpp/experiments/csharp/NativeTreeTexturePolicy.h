#pragma once
#include <array>
#include <string>
#include <string_view>

namespace bl::csharp::tree_texture {
inline bool validStem(std::string_view stem) {
    if(stem.empty()||stem.size()>160||stem.front()=='/'||stem.back()=='/')return false;
    bool segmentStart=true;
    for(char ch:stem) {
        if(ch=='/') {
            if(segmentStart)return false;
            segmentStart=true;
            continue;
        }
        if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
             (ch>='0'&&ch<='9')||ch=='_'||ch=='-'))return false;
        segmentStart=false;
    }
    return true;
}
inline bool validDimensions(int trunkW,int trunkH,int topW,int topH,
                            int branchW,int branchH) {
    constexpr int maxSize=4096;
    return trunkW>=176&&trunkW<=maxSize&&trunkH>=16&&trunkH<=maxSize&&
           topW>=80&&topW<=maxSize&&topH>=80&&topH<=maxSize&&
           branchW>=80&&branchW<=maxSize&&branchH>=40&&branchH<=maxSize;
}
inline std::array<std::string,3> paths(std::string_view modDirectory,
                                        std::string_view stem) {
    if(!validStem(stem)||modDirectory.empty())return {};
    const std::string base=std::string(modDirectory)+"/content/"+std::string(stem);
    return {base+".png",base+"_Tops.png",base+"_Branches.png"};
}
}
