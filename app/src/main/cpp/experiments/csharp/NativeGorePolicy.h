#pragma once
#include <string>
#include <string_view>
#include "experiments/csharp/NativeTreeTexturePolicy.h"
namespace bl::csharp::gore_policy {
inline bool valid(std::string_view name,std::string_view texture,int ai) {
    if(name.empty()||name.size()>72||!tree_texture::validStem(texture)||
       ai<0||ai>255)return false;
    for(char ch:name)if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
                         (ch>='0'&&ch<='9')||ch=='_'))return false;
    return true;
}
inline std::string filePath(std::string_view root,std::string_view stem) {
    return root.empty()||!tree_texture::validStem(stem)?std::string():
           std::string(root)+"/content/"+std::string(stem)+".png";
}
inline bool capacity(unsigned long long start,unsigned long long count) {
    return start>0&&count>0&&count<=256&&start+count<=32767;
}
}