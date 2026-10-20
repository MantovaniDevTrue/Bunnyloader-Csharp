#pragma once
#include "il2cpp/FieldContract.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace bl::csharp::tile_flags {
inline constexpr int supportedMask = 1048575;

using Resize = Il2CppArray* (*)(Il2CppArray*, uintptr_t);
inline bool appendHousing(int type, Il2CppClass* room, Resize resize, std::string* error) {
    auto& a = il2cpp::api();
    if (type < 0 || type > 65535 || !resize || !a.field_static_get_value || !a.field_static_set_value) {
        *error = "Stage=RegisterHousing Field=CountsAsTableTypes Reason=InvalidTypeOrMissingExport";
        return false;
    }
    FieldInfo* field = nullptr;
    if (!il2cpp::nativeFieldContract(room, "CountsAsTableTypes", "System.Int32[]", true,
                                    sizeof(void*), &field, nullptr, error)) return false;
    Il2CppArray* array = nullptr;
    a.field_static_get_value(field, &array);
    if (!il2cpp::nativeArrayContract(array, "Int32", 4, 4096)) {
        *error = "Stage=RegisterHousing Field=CountsAsTableTypes Expected=Rank1:Int32:4Bytes ActualLength=" +
            (array ? std::to_string(array->length) : "Null") + " Reason=NativeArrayContractMismatch";
        return false;
    }
    const auto* values = static_cast<const int32_t*>(arrayData(array));
    for (uintptr_t i = 0; i < array->length; ++i) if (values[i] == type) return true;
    if (array->length == 4096) {
        *error = "Stage=RegisterHousing Field=CountsAsTableTypes Expected=CapacityBelow4096 Actual=4096 Reason=CapacityExceeded";
        return false;
    }
    Il2CppArray* enlarged = resize(array, array->length + 1);
    if (enlarged == array || !il2cpp::nativeArrayContract(enlarged, "Int32", 4, 4096) ||
        enlarged->length != array->length + 1 ||
        std::memcmp(arrayData(array), arrayData(enlarged), array->length * sizeof(int32_t))) {
        *error = "Stage=RegisterHousing Field=CountsAsTableTypes Expected=ResizedCopyPreservingValues Reason=InvalidResizedArray";
        return false;
    }
    static_cast<int32_t*>(arrayData(enlarged))[array->length] = type;
    a.field_static_set_value(field, enlarged);
    return true;
}

// Called on the content/game thread. Resolve every requested array before the
// first write: a missing flag must not leave half of this declaration applied.
inline bool apply(int type, int flags, std::string* error) {
    if (type < 0 || flags < 0 || (flags & ~supportedMask)) {
        *error = "Stage=ValidateFlags Expected=NonNegativeTypeAndKnownFlags ActualType=" +
            std::to_string(type) + " ActualFlags=" + std::to_string(flags) +
            " Reason=OutOfRange";
        return false;
    }
    auto& a = il2cpp::api();
    if (!a.field_static_get_value) {
        *error = "Stage=BindFlags Reason=RequiredExportMissing Export=field_static_get_value";
        return false;
    }
    struct Flag { int bit; const char* owner; const char* name; };
    constexpr Flag definitions[] = {
        {1,"Main","tileTable"}, {2,"Main","tileSolidTop"},
        {4,"Main","tileNoAttach"}, {8,"Main","tileLavaDeath"},
        {8,"Main","tileObsidianKill"}, {16,"Main","tileFrameImportant"},
        {32,"Sets","DisableSmartCursor"}, {64,"Sets","HasOutlines"},
        {128,"Sets","IgnoredByNpcStepUp"},
        {512,"Main","tileSolid"}, {512,"Main","tileNoSunLight"},
        {1024,"Main","tileBlockLight"},
        {2048,"Main","tileMergeDirt"}, {4096,"Main","tileLighted"},
        {8192,"Main","tileWaterDeath"}, {16384,"Sets","ChecksForMerge"},
        {32768,"Main","tileCut"},
        {65536,"Sets","Campfires"}, {65536,"Sets","InteractableByNPCs"},
        {65536,"Wiring","IsAMechanism"},
        {65536,"Wiring","IgnoreWhenValidatingTraps"},
        {262144,"Sets","CommonSapling"},
        {524288,"Sets","SwaysInWindBasic"}
    };
    // CountsAsTable (256) is an integer list in RoomNeeds, not a bool indexed
    // by tile ID. Its registration remains in appendNativeHousingTable.
    std::array<uint8_t*, std::size(definitions)> staged{};
    size_t count = 0;
    for (const auto& def : definitions) {
        if (!(flags & def.bit)) continue;
        const std::string_view scope(def.owner);
        Il2CppClass* owner=nullptr;
        if(scope=="Main")owner=il2cpp::findClass({"Terraria","Main",{}});
        else {
            auto* sets=il2cpp::findClass({"Terraria.ID","TileID","Sets"});
            owner=scope=="Wiring"?il2cpp::findNested(sets,"Wiring"):sets;
        }
        FieldInfo* field = nullptr;
        std::string contract;
        if (!il2cpp::nativeFieldContract(owner, def.name, "System.Boolean[]", true,
                                        sizeof(void*), &field, nullptr, &contract)) {
            *error = "Stage=BindFlags Owner=" + std::string(def.owner) + " " + contract;
            return false;
        }
        Il2CppArray* array = nullptr;
        a.field_static_get_value(field, &array);
        if (!il2cpp::nativeArrayContract(array, "Boolean", 1, 65536) ||
            static_cast<uintptr_t>(type) >= array->length) {
            *error = "Stage=BindFlags Owner=" + std::string(def.owner) + " Field=" + def.name +
                " Expected=Rank1:Boolean:1Byte:IndexInBounds ActualLength=" +
                (array ? std::to_string(array->length) : "Null") +
                " Index=" + std::to_string(type) + " Reason=NativeArrayContractMismatch";
            return false;
        }
        staged[count++] = static_cast<uint8_t*>(arrayData(array)) + type;
    }
    for (size_t i = 0; i < count; ++i) *staged[i] = 1;
    return true;
}
}
