#include "experiments/csharp/NativeCloudBiomeSampler.h"
#include "experiments/csharp/NativeBiomeManifest.h"

#include "il2cpp/Api.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <set>
#include <string>

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

// Original ExampleBiomeTileCount.TileCountsAvailable reads the local
// Main.PlayerSceneMetrics._tileCounts[ModContent.TileType("ExampleTile")].
// This is O(1) and runs at most once per biome snapshot, NOT a tile scan.
int sampleExampleTileCount(Il2CppClass* main) {
    if (!bl::runtime::modTilesSettled()) return -1;
    const int type = bl::runtime::modTileTypeByName(
        "ExampleModCSharp", "ExampleTile");
    if (type < 0 || type >= 65536) return -1;

    Il2CppClass* sceneClass = il2cpp::findClass(
        {"Terraria", "SceneMetrics", {}});
    if (!sceneClass) return -1;
    FieldInfo* metricsField = nullptr;
    int countsOffset = -1;
    std::string why;
    if (!il2cpp::nativeFieldContract(main, "PlayerSceneMetrics",
            "Terraria.SceneMetrics", true, sizeof(void*),
            &metricsField, nullptr, &why) ||
        !il2cpp::nativeFieldContract(sceneClass, "_tileCounts",
            "System.Int32[]", false, sizeof(void*),
            nullptr, &countsOffset, &why))
        return -1;

    auto& api = il2cpp::api();
    Il2CppObject* scene = nullptr;
    api.field_static_get_value(metricsField, &scene);
    if (!scene || !api.object_get_class ||
        api.object_get_class(scene) != sceneClass)
        return -1;

    Il2CppArray* counts = *reinterpret_cast<Il2CppArray**>(
        reinterpret_cast<char*>(scene) + countsOffset);
    if (!il2cpp::nativeArrayContract(counts, "Int32", sizeof(int32_t),
                                     65536) ||
        static_cast<uintptr_t>(type) >= counts->length)
        return -1;
    const int32_t count = static_cast<int32_t*>(arrayData(counts))[type];
    return count >= 0 ? count : -1;
}

} // namespace

bool readNativeBiomeTileCountsWire(const std::string& owner,
    const std::vector<std::string>& countedTiles,
    std::string* output, std::string* error) {
    if (error) error->clear();
    if (!output || !validNativeBiomeIdentifier(owner, true) ||
        countedTiles.size() > 32)
        return fail(error, "BiomeTiles: invalid owner or tile list");

    std::set<std::string> names;
    for (const auto& name : countedTiles)
        if (!validNativeBiomeIdentifier(name, false) ||
            !names.insert(name).second)
            return fail(error, "BiomeTiles: duplicate or unsafe tile name");

    output->clear();
    if (countedTiles.empty()) return true;
    if (!bl::runtime::modTilesSettled())
        return fail(error, "BiomeTiles: native tile tables not settled");

    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* metrics = il2cpp::findClass({"Terraria", "SceneMetrics", {}});
    if (!main || !metrics)
        return fail(error, "BiomeTiles: missing SceneMetrics");

    FieldInfo* sceneField = nullptr;
    int countsOffset = -1;
    if (!il2cpp::nativeFieldContract(main, "PlayerSceneMetrics",
            "Terraria.SceneMetrics", true, sizeof(void*),
            &sceneField, nullptr, error) ||
        !il2cpp::nativeFieldContract(metrics, "_tileCounts",
            "System.Int32[]", false, sizeof(void*),
            nullptr, &countsOffset, error))
        return false;

    auto& a = il2cpp::api();
    if (!a.field_static_get_value || !a.object_get_class)
        return fail(error, "BiomeTiles: missing runtime exports");
    Il2CppObject* scene = nullptr;
    a.field_static_get_value(sceneField, &scene);
    if (!scene || a.object_get_class(scene) != metrics)
        return fail(error, "BiomeTiles: player metrics not available");

    auto* tileCounts = *reinterpret_cast<Il2CppArray**>(
        reinterpret_cast<char*>(scene) + countsOffset);
    if (!il2cpp::nativeArrayContract(tileCounts, "Int32",
                                    sizeof(int32_t), 65536))
        return fail(error, "BiomeTiles: invalid native count table");

    std::string staged;
    for (const auto& name : countedTiles) {
        const int type = bl::runtime::modTileTypeByName(owner, name);
        if (type < 0 || static_cast<uintptr_t>(type) >= tileCounts->length)
            return fail(error, "BiomeTiles: missing registered native tile");
        const int count = static_cast<int32_t*>(arrayData(tileCounts))[type];
        if (count < 0 || count > 1000000)
            return fail(error, "BiomeTiles: invalid tile count");
        if (!staged.empty()) staged.push_back(';');
        staged += name + "=" + std::to_string(count);
        if (staged.size() > 4096)
            return fail(error, "BiomeTiles: snapshot too large");
    }
    *output = std::move(staged);
    return true;
}

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
    FieldInfo* menuField = nullptr;
    FieldInfo* anniversaryField = nullptr;
    int activeOffset = -1, skyOffset = -1;
    int surfaceOffset = -1, positionOffset = -1;
    if (!fieldArrayIsPlayer(main, player, &playersField) ||
        !il2cpp::nativeFieldContract(main, "myPlayer", "System.Int32",
            true, sizeof(int32_t), &localIndexField, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "maxTilesX", "System.Int32",
            true, sizeof(int32_t), &tilesXField, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "gameMenu", "System.Boolean",
            true, sizeof(uint8_t), &menuField, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "tenthAnniversaryWorld", "System.Boolean",
            true, sizeof(uint8_t), &anniversaryField, nullptr, error) ||
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
    uint8_t inMenu = 1, anniversary = 0;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(menuField, &inMenu);
    api.field_static_get_value(anniversaryField, &anniversary);
    if (inMenu) return fail(error, "CloudBiome: main menu is active");
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
    facts->tenthAnniversaryWorld = anniversary != 0;
    facts->zoneSkyHeight = sky;
    facts->zoneOverworldHeight = surface;
    facts->playerPositionX = pos.x;
    facts->maxTilesX = maxTilesX;
    facts->exampleBiomeTileCount = sampleExampleTileCount(main);
    // No BunnyBiome/ModSystem registry exists in the CSharp host yet.
    // Therefore even a tile count of 40 does not authorize the mod key.
    facts->exampleBiomeRegistered = false;
    return true;
}

} // namespace bl::csharp
