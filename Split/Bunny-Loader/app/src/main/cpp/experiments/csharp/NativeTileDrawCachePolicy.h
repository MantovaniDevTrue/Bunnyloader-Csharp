#pragma once
namespace bl::csharp::tile_draw_cache {
inline constexpr bool enabled(bool special,bool jsUncached,bool managedUncached) {
    return !special && !jsUncached && !managedUncached;
}
}
