#include "experiments/csharp/NativeInvasionSpawnSite.h"

#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bl::csharp {

bool findNativeInvasionSpawnSite(int preferredX, int preferredY,
                                InvasionSpawnSite* result) {
    if (!result || preferredX < 0 || preferredY < 0) return false;
    *result = {};
    runtime::TileArrays world;
    if (!runtime::tileArrays(&world)) return false;

    static FieldInfo* solidField = nullptr;
    static FieldInfo* topField = nullptr;
    if (!solidField || !topField) {
        auto* main = il2cpp::findClass({"Terraria", "Main", {}});
        if (!main) return false;
        solidField = il2cpp::findField(main, "tileSolid");
        topField = il2cpp::findField(main, "tileSolidTop");
        if (!solidField || !topField) return false;
    }
    Il2CppArray* solid = nullptr;
    Il2CppArray* solidTop = nullptr;
    auto& a = il2cpp::api();
    a.field_static_get_value(solidField, &solid);
    a.field_static_get_value(topField, &solidTop);
    if (!solid || !solidTop) return false;
    InvasionTileView view;
    view.width = world.width;
    view.height = world.height;
    view.lookup = world.lookup;
    view.headers = world.sHeader;
    view.type = world.type;
    view.solid = static_cast<const uint8_t*>(arrayData(solid));
    view.solidTop = static_cast<const uint8_t*>(arrayData(solidTop));
    view.solidCount = solid->length;
    view.solidTopCount = solidTop->length;
    static const MethodInfo* collisionMethod = nullptr;
    if (!collisionMethod) {
        auto* collision = il2cpp::findClass({"Terraria", "Collision", {}});
        if (collision)
            collisionMethod = il2cpp::findMethodBySignature(collision,
                il2cpp::parseSignature(
                    "bool SolidCollision(Vector2 Position, int Width, int Height)"));
    }
    if (!collisionMethod) return false;
    struct Vec2 { float x, y; };
    *result = findInvasionSpawnSiteChecked(view, preferredX, preferredY,
        [&](int x, int groundY) {
            Vec2 position{static_cast<float>(x - 40),
                          static_cast<float>(groundY - 96)};
            int width = 80;
            int height = 88;
            void* args[] = {&position, &width, &height};
            Il2CppObject* exception = nullptr;
            Il2CppObject* boxed = a.runtime_invoke(
                collisionMethod, nullptr, args, &exception);
            if (exception || !boxed) return false;
            const auto* bytes = reinterpret_cast<const uint8_t*>(boxed);
            return bytes[sizeof(Il2CppObject)] == 0;
        });
    return result->valid;
}

}
