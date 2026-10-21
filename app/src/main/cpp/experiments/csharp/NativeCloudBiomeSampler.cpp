#include "experiments/csharp/NativeCloudBiomeSampler.h"

#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace bl::csharp {
namespace {

struct Vector2Native {
    float x = 0;
    float y = 0;
};

bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}

bool fieldArrayIsPlayer(Il2CppClass* main, Il2CppClass* player,
                        FieldInfo** field) {
    auto& api = il2cpp::api();
    if (!api.field_get_flags || !api.field_get_type ||
        !api.class_from_il2cpp_type || !api.class_get_rank ||
        !api.class_get_element_class || !api.field_static_get_value)
        return false;
    auto* value = il2cpp::findField(main, "player");
    if (!value || !(api.field_get_flags(value) & 0x10u))
        return false;
    auto* arrayClass = api.class_from_il2cpp_type(api.field_get_type(value));
    if (!arrayClass || api.class_get_rank(arrayClass) != 1 ||
        api.class_get_element_class(arrayClass) != player)
        return false;
    *field = value;
    return true;
}

} // namespace

bool readNativeCloudVanillaBiomeFacts(bool gameMenu,
                                     NativeCloudBiomeFacts* facts,
                                     std::string* error) {
    if (error) error->clear();
    if (!facts) return fail(error, "CloudBiome: null result");
    *facts = {};
    facts->gameMenu = gameMenu;
    // No local world/player snapshot may be used from the main menu.
    if (gameMenu) return false;

    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.class_get_element_class ||
        !api.class_get_rank)
        return fail(error, "CloudBiome: IL2CPP field APIs missing");

    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    if (!main || !player)
        return fail(error, "CloudBiome: Terraria.Main/Player missing");

    FieldInfo *playersField = nullptr, *localIndexField = nullptr;
    FieldInfo* tilesXField = nullptr;
    int activeOffset = -1, skyOffset = -1;
    int surfaceOffset = -1, positionOffset = -1;
    if (!fieldArrayIsPlayer(main, player, &playersField) ||
        !il2cpp::nativeFieldContract(main, "myPlayer", "System.Int32",
            true, sizeof(int32_t), &localIndexField, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "maxTilesX", "System.Int32",
            true, sizeof(int32_t), &tilesXField, nullptr, error) ||
        !il2cpp::nativeFieldContract(player, "active", "System.Boolean",
            false, sizeof(uint8_t), nullptr, &activeOffset, error) ||
        !il2cpp::nativeFieldContract(player, "ZoneSkyHeight", "System.Boolean",
            false, sizeof(uint8_t), nullptr, &skyOffset, error) ||
        !il2cpp::nativeFieldContract(player, "ZoneOverworldHeight", "System.Boolean",
            false, sizeof(uint8_t), nullptr, &surfaceOffset, error) ||
        !il2cpp::nativeFieldContract(player, "position", "Microsoft.Xna.Framework.Vector2",
            false, sizeof(Vector2Native), nullptr, &positionOffset, error))
        return false;

    int32_t index = -1, maxTilesX = 0;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(localIndexField, &index);
    api.field_static_get_value(tilesXField, &maxTilesX);
    api.field_static_get_value(playersField, &players);
    if (!players || index < 0 || index >= 255 ||
        index >= static_cast<int32_t>(players->length) ||
        players->length > 256 || maxTilesX <= 0 || maxTilesX > 100000 ||
        api.class_get_rank(players->obj.klass) != 1 ||
        api.class_get_element_class(players->obj.klass) != player)
        return fail(error, "CloudBiome: invalid local player array/index");

    Il2CppObject* local =
        static_cast<Il2CppObject**>(arrayData(players))[index];
    if (!local)
        return fail(error, "CloudBiome: local player missing");

    const auto* object = reinterpret_cast<const char*>(local);
    const bool active = *reinterpret_cast<const uint8_t*>(object + activeOffset) != 0;
    const bool sky = *reinterpret_cast<const uint8_t*>(object + skyOffset) != 0;
    const bool surface = *reinterpret_cast<const uint8_t*>(object + surfaceOffset) != 0;
    Vector2Native pos{};
    std::memcpy(&pos, object + positionOffset, sizeof(pos));
    if (!active || !std::isfinite(pos.x))
        return fail(error, "CloudBiome: invalid local player or coordinates");

    facts->playerActive = true;
    facts->gameMenu = false;
    facts->zoneSkyHeight = sky;
    facts->zoneOverworldHeight = surface;
    facts->playerPositionX = pos.x;
    facts->maxTilesX = maxTilesX;
    facts->exampleBiomeTileCount = -1;
    return true;
}

} // namespace bl::csharp
