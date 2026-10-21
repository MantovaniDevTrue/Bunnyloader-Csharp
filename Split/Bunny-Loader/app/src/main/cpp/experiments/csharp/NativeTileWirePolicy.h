#pragma once
#include <cstddef>
namespace bl::csharp::tile_wire {
inline constexpr bool supported(std::size_t n) {
    return n==13||n==14||n==16||n==17||n==19||n==20||n==21||n==22||n==23||n==25||n==28||n==29;
}
inline constexpr bool hasSaplingShape(std::size_t n) {return n==28||n==29;}
inline constexpr bool hasSaplingGrowth(std::size_t n) {return n==29;}
inline constexpr bool hasMiningPolicy(std::size_t n) {return n==25||n==28||n==29;}
inline constexpr bool hasAnimationHeight(std::size_t n) {return supported(n)&&n>=14;}
inline constexpr bool hasAnimationPolicy(std::size_t n) {return n==16||n==17||n==19||n==20||n==21||n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasAdjacencies(std::size_t n) {return n==17||n==19||n==20||n==21||n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasBreakPolicy(std::size_t n) {return n==19||n==20||n==21||n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasCallbacks(std::size_t n) {return n==20||n==21||n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasDrawCachePolicy(std::size_t n) {return n==21||n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasFrameToggle(std::size_t n) {return n==22||n==23||n==25||n==28||n==29;}
inline constexpr bool hasRegisteredDrops(std::size_t n) {return n==23||n==25||n==28||n==29;}
}
