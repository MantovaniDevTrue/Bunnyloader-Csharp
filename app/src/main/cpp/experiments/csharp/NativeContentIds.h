#pragma once
#include <string>
#include <string_view>
#include "il2cpp/FieldContract.h"

namespace bl::csharp {
// ItemRarityID is a tModLoader constant table, absent from this Terraria Android.
// Public ID values: tModLoader (MIT), pinned reference:
// https://github.com/tModLoader/tModLoader/blob/c9545cf27d56c456321e1896274c02adf67d0140/patches/tModLoader/Terraria/ID/ItemRarityID.cs
// Android ItemRarityColor also confirms Cyan9, StrongRed10, Purple11, etc.
// These are IDs, not native offsets or a PC runtime dependency. Keep negative
// special rarities signed; they are valid Item.rare values. Expert/Master flags
// remain separate item defaults, just as in tModLoader.
inline bool resolveNativeContentId(const std::string& family,const std::string& name,
                                   int* value,std::string* error) {
    if(family=="ItemRarityID") {
        struct Constant {std::string_view name;int value;};
        constexpr Constant ids[]={
            {"Master",-13},{"Expert",-12},{"Quest",-11},{"Gray",-1},
            {"White",0},{"Blue",1},{"Green",2},{"Orange",3},{"LightRed",4},
            {"Pink",5},{"LightPurple",6},{"Lime",7},{"Yellow",8},{"Cyan",9},
            {"Red",10},{"Purple",11},{"Count",12}
        };
        for(const auto& id:ids)if(id.name==name){*value=id.value;return true;}
        *error="Field="+name+" Reason=UnknownItemRarityId";return false;
    }
    // Other supported families exist in Android. Preserve native type/range
    // checks: a missing/malformed native constant must never become ID zero.
    if(family!="AmmoID"&&family!="ProjectileID"&&family!="ItemID"&&family!="ItemUseStyleID") {
        *error="Family="+family+" Reason=UnsupportedIdFamily";return false;
    }
    return il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID",family,{}}),name.c_str(),value,error);
}
}
