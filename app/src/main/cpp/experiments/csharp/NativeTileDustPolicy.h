#pragma once
#include <charconv>
#include <string_view>
namespace bl::csharp::tile_dust {
inline bool parseNumber(std::string_view s,int lower,int upper,int* result) {
    if(!result||s.empty()||s.size()>5)return false;
    int n=0;
    const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
    if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size()||n<lower||n>upper)return false;
    *result=n;return true;
}
inline bool parseCount(std::string_view s,int* result) {
    return s.substr(0,5)=="DUST|"&&parseNumber(s.substr(5),0,20,result);
}
inline bool parseCreate(std::string_view s,bool* create,int* result) {
    if(!create||!result||s.size()<10||s.substr(0,7)!="CREATE|"||
       (s[7]!='0'&&s[7]!='1')||s[8]!='|')return false;
    int num=0;
    if(!parseNumber(s.substr(9),-1,1999,&num))return false;
    *create=s[7]=='1';*result=num;return true;
}
}
