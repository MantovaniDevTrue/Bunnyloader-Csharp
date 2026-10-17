#include "graphics/ScreenScene.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cmath>

namespace bl::graphics {
namespace {

struct Vec2 { float x, y; };

struct Fields {
    bool initialized = false;
    FieldInfo* daytime = nullptr;
    FieldInfo* time = nullptr;
    FieldInfo* screenWidth = nullptr;
    FieldInfo* screenHeight = nullptr;
    FieldInfo* screenPosition = nullptr;
    FieldInfo* worldSurface = nullptr;
    FieldInfo* moonPhase = nullptr;
};

Fields g_fields;

template <typename T>
bool read(FieldInfo* field, T& out) {
    if (!field) return false;
    il2cpp::api().field_static_get_value(field, &out);
    return true;
}

}

ScreenSceneSnapshot sampleScreenScene() {
    if (!g_fields.initialized) {
        g_fields.initialized = true;
        Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
        if (main) {
            g_fields.daytime = il2cpp::findField(main, "dayTime");
            g_fields.time = il2cpp::findField(main, "time");
            g_fields.screenWidth = il2cpp::findField(main, "screenWidth");
            g_fields.screenHeight = il2cpp::findField(main, "screenHeight");
            g_fields.screenPosition = il2cpp::findField(main, "screenPosition");
            g_fields.worldSurface = il2cpp::findField(main, "worldSurface");
            g_fields.moonPhase = il2cpp::findField(main, "moonPhase");
        }
    }

    ScreenSceneSnapshot s;
    bool day = true;
    double gameTime = 0.0;
    double surface = 0.0;
    int width = 0, height = 0, moonPhase = 0;
    Vec2 pos{};
    if (!read(g_fields.daytime, day) || !read(g_fields.time, gameTime) ||
        !read(g_fields.screenWidth, width) || !read(g_fields.screenHeight, height) ||
        !read(g_fields.screenPosition, pos) || !read(g_fields.worldSurface, surface) ||
        !std::isfinite(gameTime) || !std::isfinite(surface) ||
        !std::isfinite(pos.y) || width <= 0 || height <= 0 || surface <= 0.0)
        return s;

    read(g_fields.moonPhase, moonPhase);
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float duration = day ? 27000.f : 16200.f;
    const float progress = std::clamp(static_cast<float>(gameTime) / duration, 0.f, 2.f);
    const float p = 1.f - progress;
    const float worldSurfacePixels = static_cast<float>(surface) * 16.f;
    const float denominator = worldSurfacePixels - 600.f;
    const float correction = std::fabs(denominator) > 0.01f ? -pos.y / denominator * 200.f : 0.f;
    const float sunX = (progress * w * 0.5f - 100.f * p) / w;
    const float sunY = 1.f - (correction + p * p * 250.f + 180.f) / h;
    const float depth = (pos.y + h * 0.5f) / 16.f - (static_cast<float>(surface) - 150.f);
    const float attenuation = std::max(0.f, day ? 1.f - std::max(depth, 0.f) / 600.f :
                                                1.f - std::max(depth, 0.f) / 200.f);
    const float moon = moonPhase == 0 ? 1.01f :
                       (moonPhase == 3 || moonPhase == 5) ? .9f :
                       moonPhase == 4 ? .6f : 1.f;

    s.valid = true;
    s.daytime = day;
    s.sunEnabled = pos.y + h * 0.5f < worldSurfacePixels + 800.f;
    s.sunX = std::clamp(sunX, -1.f, 2.f);
    s.sunY = std::clamp(sunY, -1.f, 2.f);
    s.clock = std::clamp(static_cast<float>(gameTime) / (day ? 54000.f : 32400.f), 0.f, 1.f);
    s.strength = 0.8f * (day ? 1.f : moon) * attenuation;
    s.shadowThreshold = day ? 0.98f : 0.02f;
    return s;
}

}
