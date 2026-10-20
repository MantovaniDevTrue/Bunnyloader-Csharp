#include "experiments/csharp/NativeTileAdjacencyPolicy.h"
#include <array>
#include <cassert>
#include <iostream>
#include <utility>
int main(){
 using namespace bl::csharp::tile_adjacency;
 std::array<std::pair<int,int>,4> pairs{};size_t n=0;
 assert(add(pairs,n,762,18));assert(add(pairs,n,760,80));assert(add(pairs,n,762,16));
 assert(add(pairs,n,762,18));assert(n==3);
 assert(pairs[0]==(std::pair<int,int>{760,80}));
 assert(pairs[1]==(std::pair<int,int>{762,16}));
 assert(pairs[2]==(std::pair<int,int>{762,18}));
 std::array<bool,754> flags{};
 assert(forEach(pairs,n,762,[&](int alias){flags[alias]=true;}));
 assert(flags[16]&&flags[18]&&!flags[80]);
 assert(!forEach(pairs,n,761,[&](int){assert(false);}));
 assert(!add(pairs,n,754,-1));assert(!add(pairs,n,753,18));assert(!add(pairs,n,762,754));
 assert(add(pairs,n,760,20));assert(n==4);
 assert(!add(pairs,n,760,21));assert(n==4);
 assert(forEach(pairs,n,760,[&](int alias){flags[alias]=true;}));
 assert(flags[20]&&flags[80]);
 std::cout<<"CSHARP_TILE_ADJACENCIES_SORTED_MULTIPLE_NO_QUICKJS=PASS\n";
}
