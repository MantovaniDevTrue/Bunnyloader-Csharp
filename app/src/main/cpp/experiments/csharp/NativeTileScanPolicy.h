#pragma once
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace bl::csharp::tile_scan {
inline constexpr std::size_t scanBudget(std::size_t total) {
    return total < 512u ? total : 512u;
}
inline constexpr std::size_t nextCursor(std::size_t first,std::size_t visited,std::size_t total) {
    return total ? (first%total+visited%total)%total : 0u;
}
inline constexpr uint64_t cellKey(int x,int y,bool visual=false) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x))<<33)|
           (static_cast<uint64_t>(static_cast<uint32_t>(y))<<1)|
           (visual?1ull:0ull);
}
inline bool parseLight(std::string_view reply,int* r,int* g,int* b) {
    if(!r||!g||!b||reply.size()>48||reply.size()<11||
       reply.substr(0,6)!="LIGHT|")return false;
    int values[3]{};
    size_t begin=6;
    for(int i=0;i<3;i++) {
        const auto end=reply.find('|',begin);
        if((i<2&&end==std::string_view::npos)||
           (i==2&&end!=std::string_view::npos))return false;
        const size_t finish=i==2?reply.size():end;
        const auto first=reply.data()+begin,last=reply.data()+finish;
        const auto parsed=std::from_chars(first,last,values[i]);
        if(first==last||parsed.ec!=std::errc{}||parsed.ptr!=last||
           values[i]<0||values[i]>4000)return false;
        begin=finish+1;
    }
    *r=values[0];*g=values[1];*b=values[2];
    return true;
}
}
