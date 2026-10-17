#include "graphics/ScreenScene.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace bl::graphics {
namespace {

struct Vec2 { float x, y; };

enum class NumericType { Unknown, Single, Double };

NumericType numericType(FieldInfo* field) {
    if (!field) return NumericType::Unknown;
    auto& api = il2cpp::api();
    const Il2CppType* type = api.field_get_type(field);
    char* name = type ? api.type_get_name(type) : nullptr;
    const NumericType result = name && std::strcmp(name, "System.Single") == 0
        ? NumericType::Single : name && std::strcmp(name, "System.Double") == 0
        ? NumericType::Double : NumericType::Unknown;
    if (name) api.il2cpp_free(name);
    return result;
}

struct Fields {
    NumericType clockType = NumericType::Unknown;
    NumericType surfaceType = NumericType::Unknown;
    bool initialized = false;
    bool resolved = false;
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

bool readDouble(FieldInfo* field, NumericType type, double& out) {
    if (!field) return false;
    auto& api = il2cpp::api();
    if (type == NumericType::Single) {
        float value = 0.f;
        api.field_static_get_value(field, &value);
        out = value;
        return std::isfinite(value);
    }
    if (type != NumericType::Double) return false;
    api.field_static_get_value(field, &out);
    return std::isfinite(out);
}

}

ScreenSceneSnapshot sampleScreenScene() {
    if (!g_fields.initialized) {
        Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
        if (main) {
            g_fields.daytime = il2cpp::findField(main, "dayTime");
            g_fields.time = il2cpp::findField(main, "time");
            g_fields.screenWidth = il2cpp::findField(main, "screenWidth");
            g_fields.screenHeight = il2cpp::findField(main, "screenHeight");
            g_fields.screenPosition = il2cpp::findField(main, "screenPosition");
            g_fields.worldSurface = il2cpp::findField(main, "worldSurface");
            g_fields.moonPhase = il2cpp::findField(main, "moonPhase");
            g_fields.clockType = numericType(g_fields.time);
            g_fields.surfaceType = numericType(g_fields.worldSurface);
            g_fields.initialized = true;
            g_fields.resolved = true;
        }
    }

    ScreenSceneSnapshot s;
    bool day = true;
    double gameTime = 0.0;
    double surface = 0.0;
    int width = 0, height = 0, moonPhase = 0;
    Vec2 pos{};
    const bool dayReady = read(g_fields.daytime, day);
    const bool timeReady = readDouble(g_fields.time, g_fields.clockType, gameTime) && gameTime >= 0.0;
    const bool widthReady = read(g_fields.screenWidth, width) && width > 0 && width < 32768;
    const bool heightReady = read(g_fields.screenHeight, height) && height > 0 && height < 32768;
    const bool posReady = read(g_fields.screenPosition, pos) &&
                          std::isfinite(pos.x) && std::isfinite(pos.y);
    const bool surfaceReady = readDouble(g_fields.worldSurface, g_fields.surfaceType, surface) &&
                              surface > 0.0 && surface < 100000.0;
    if (!dayReady || !timeReady || !widthReady || !heightReady ||
        !posReady || !surfaceReady) {
        static bool notified = false;
        if (!notified) {
            notified = true;
            BL_WARN("CSharp ScreenFx: dados parciais do Terraria (Main=%d day=%d time=%d size=%d/%d pos=%d surface=%d); grafo continua com valores seguros",
                    g_fields.resolved, dayReady, timeReady, widthReady, heightReady,
                    posReady, surfaceReady);
        }
    }
    if (!timeReady) gameTime = 13500.0;
    if (!widthReady) width = 1920;
    if (!heightReady) height = 1080;
    if (!posReady) pos = Vec2{};
    if (!surfaceReady) surface = 250.0;
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
