#pragma once
#include <charconv>
#include <string_view>
namespace bl::csharp::tile_drop {
struct Item {int type=0,stack=0;};
struct Drops {bool custom=false;Item first{},second{};};
inline bool readInt(std::string_view s,int lo,int hi,int* result) {
    if(!result||s.empty()||s.size()>5)return false;
    int n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||n<lo||n>hi)return false;
    *result=n;return true;
}
inline bool readItem(std::string_view s,Item* item) {
    if(!item)return false;
    const size_t comma=s.find(',');
    if(comma==std::string_view::npos||s.find(',',comma+1)!=std::string_view::npos)return false;
    Item local{};
    if(!readInt(s.substr(0,comma),1,65535,&local.type)||
       !readInt(s.substr(comma+1),1,9999,&local.stack))return false;
    *item=local;return true;
}
inline bool parseReply(std::string_view s,Drops* result) {
    if(!result)return false;
    if(s=="DEFAULT"){*result={};return true;}
    if(s.substr(0,6)!="DROPS|"||s.size()<7)return false;
    const char n=s[6];
    if(n<'0'||n>'2')return false;
    Drops temp{};temp.custom=true;
    if(n=='0'){if(s.size()!=7)return false;*result=temp;return true;}
    if(s.size()<10||s[7]!='|')return false;
    const auto rem=s.substr(8);
    const size_t separator=rem.find('|');
    if(n=='1') {
        if(separator!=std::string_view::npos||!readItem(rem,&temp.first))return false;
    } else {
        if(separator==std::string_view::npos||
           !readItem(rem.substr(0,separator),&temp.first)||
           !readItem(rem.substr(separator+1),&temp.second))return false;
    }
    *result=temp;return true;
}
}
