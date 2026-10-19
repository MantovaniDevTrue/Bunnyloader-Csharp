#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
namespace bl::csharp::tile_adjacency {
template<size_t N>
bool add(std::array<std::pair<int,int>,N>& data,size_t& count,int type,int alias) {
    if(type<754||alias<0||alias>=754||count>N)return false;
    auto begin=data.begin(),end=begin+count;
    const auto key=std::pair<int,int>{type,alias};
    auto it=std::lower_bound(begin,end,key);
    if(it!=end&&*it==key)return true;
    if(count==N)return false;
    for(auto cursor=end;cursor!=it;--cursor)*cursor=*(cursor-1);
    *it=key;++count;
    return true;
}
template<size_t N,typename Fn>
bool forEach(const std::array<std::pair<int,int>,N>& data,size_t count,int type,Fn fn) {
    if(count>N||type<754)return false;
    auto begin=data.begin(),end=begin+count;
    auto it=std::lower_bound(begin,end,std::pair<int,int>{type,-1});
    if(it==end||it->first!=type)return false;
    for(;it!=end&&it->first==type;++it)fn(it->second);
    return true;
}
}
