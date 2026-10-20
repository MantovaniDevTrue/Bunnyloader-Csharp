#include "experiments/csharp/NativeInvasionHud.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace bl::csharp {
namespace {

using Key = std::pair<std::string, std::string>;
struct Entry {
    std::string label;
    int progress = 0;
    int goal = 1;
    int phase = 0;
    int remaining = 240;
};

std::mutex g_mutex;
std::map<Key, Entry> g_entries;
std::atomic<uint64_t> g_session{0};
std::atomic<bool> g_ready{false};

struct RectLayout {
    std::array<size_t, 4> offsets{};
    bool ready = false;
};

struct Refs {
    FieldInfo* spriteBatch = nullptr;
    FieldInfo* screenWidth = nullptr;
    FieldInfo* screenHeight = nullptr;
    FieldInfo* gameMenu = nullptr;
    FieldInfo* magicPixel = nullptr;
    const MethodInfo* drawText = nullptr;
    const MethodInfo* drawRect = nullptr;
    NativeColorLayout colors;
    RectLayout rectangle;
    bool valid = false;
};

Refs& refs() {
    static Refs r;
    if (r.valid) return r;
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* utils = il2cpp::findClass({"Terraria", "Utils", {}});
    auto* textures = il2cpp::findClass({"Terraria.GameContent", "TextureAssets", {}});
    auto* batch = il2cpp::findClass({"Microsoft.Xna.Framework.Graphics", "SpriteBatch", {}});
    auto* color = il2cpp::findClass({"Microsoft.Xna.Framework", "Color", {}});
    auto* rectangle = il2cpp::findClass({"Microsoft.Xna.Framework", "Rectangle", {}});
    if (!main || !utils || !textures || !batch || !color || !rectangle) return r;

    Refs next;
    next.spriteBatch = il2cpp::findField(main, "spriteBatch");
    next.screenWidth = il2cpp::findField(main, "screenWidth");
    next.screenHeight = il2cpp::findField(main, "screenHeight");
    next.gameMenu = il2cpp::findField(main, "gameMenu");
    next.magicPixel = il2cpp::findField(textures, "MagicPixel");
    next.drawText = il2cpp::findMethodBySignature(utils, il2cpp::parseSignature(
        "Vector2 DrawBorderString(SpriteBatch sb, string text, Vector2 pos, "
        "Color color, float scale, float anchorx, float anchory, "
        "int maxCharactersDisplayed)"));
    next.drawRect = il2cpp::findMethodBySignature(batch, il2cpp::parseSignature(
        "void Draw(Texture2D texture, Rectangle destinationRectangle, Color color)"));
    std::string error;
    il2cpp::ValueLayout layout;
    if (!next.colors.bind(color, &error) ||
        !il2cpp::resolveValueLayout(rectangle, &layout, &error) ||
        layout.size != 16) return r;
    const char* fields[] = {"X", "Y", "Width", "Height"};
    for (int i = 0; i < 4; ++i) {
        auto* field = il2cpp::findField(rectangle, fields[i]);
        if (!field || !layout.offset(il2cpp::api().field_get_offset(field),
                                     sizeof(int32_t), &next.rectangle.offsets[i]))
            return r;
    }
    next.rectangle.ready = true;
    next.valid = next.spriteBatch && next.screenWidth && next.screenHeight &&
        next.gameMenu && next.magicPixel && next.drawText && next.drawRect;
    if (next.valid) r = next;
    return r;
}

void* rectangleValue(int x, int y, int width, int height, std::array<uint8_t, 16>& buffer) {
    const int32_t values[] = {x, y, width, height};
    for (size_t index = 0; index < 4; ++index)
        std::memcpy(buffer.data() + refs().rectangle.offsets[index],
                    &values[index], sizeof(int32_t));
    return buffer.data();
}

Il2CppObject* texture() {
    const Refs& r = refs();
    Il2CppObject* asset = nullptr;
    il2cpp::api().field_static_get_value(r.magicPixel, &asset);
    if (!asset) return nullptr;
    auto* cls = il2cpp::api().object_get_class(asset);
    const MethodInfo* getter = cls ?
        il2cpp::api().class_get_method_from_name(cls, "get_Value", 0) : nullptr;
    if (!getter) return nullptr;
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = il2cpp::api().runtime_invoke(
        getter, asset, nullptr, &exception);
    return exception ? nullptr : value;
}

void drawRectangle(Il2CppObject* batch, Il2CppObject* tex,
                   int x, int y, int width, int height, uint32_t color) {
    if (!batch || !tex || width <= 0 || height <= 0) return;
    std::array<uint8_t, 16> bytes{};
    void* r = rectangleValue(x, y, width, height, bytes);
    void* args[] = {tex, r, &color};
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(refs().drawRect, batch, args, &exception);
}

void drawText(Il2CppObject* batch, const std::string& text,
              float x, float y, uint32_t color) {
    struct Point {float x, y;};
    Point position{x, y};
    float scale = 0.85f, anchorX = 0.5f, anchorY = 0.f;
    int maximum = -1;
    auto& a = il2cpp::api();
    Il2CppString* value = a.string_new(text.c_str());
    if (!value) return;
    void* args[] = {batch, value, &position, &color,
                    &scale, &anchorX, &anchorY, &maximum};
    Il2CppObject* exception = nullptr;
    a.runtime_invoke(refs().drawText, nullptr, args, &exception);
}

using Original = void (*)(const MethodInfo*);
Original g_original = nullptr;
bool g_installed = false;

void drawHud(const MethodInfo* method) {
    if (g_original) g_original(method);
    if (!g_ready.load(std::memory_order_acquire)) return;
    const Refs& r = refs();
    if (!r.valid) return;
    bool gameMenu = true;
    auto& api = il2cpp::api();
    api.field_static_get_value(r.gameMenu, &gameMenu);
    if (gameMenu) return;
    std::array<Entry, 3> visible;
    size_t count = 0;
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        for (auto it = g_entries.begin(); it != g_entries.end();) {
            if (it->second.phase == 2 && --it->second.remaining <= 0) {
                it = g_entries.erase(it);
                continue;
            }
            if (count < visible.size())
                visible[count++] = it->second;
            ++it;
        }
    }
    if (count == 0) return;
    Il2CppObject* batch = nullptr;
    int width = 0, height = 0;
    api.field_static_get_value(r.spriteBatch, &batch);
    api.field_static_get_value(r.screenWidth, &width);
    api.field_static_get_value(r.screenHeight, &height);
    if (!batch || width < 360 || height < 260) return;
    Il2CppObject* tex = texture();
    const int barWidth = std::clamp(width / 4, 180, 400);
    const int x = width / 2 - barWidth / 2;
    const int y0 = height / 7;
    const uint32_t backdrop = r.colors.pack(16, 19, 29, 210);
    const uint32_t border = r.colors.pack(250, 205, 90, 245);
    const uint32_t fill = r.colors.pack(100, 205, 110, 245);
    const uint32_t fontColor = r.colors.pack(255, 240, 190, 255);
    for (size_t i = 0; i < count; ++i) {
        const int y = y0 + static_cast<int>(i) * 53;
        const Entry& event = visible[i];
        if (tex) {
            drawRectangle(batch, tex, x - 4, y + 24, barWidth + 8, 20, backdrop);
            drawRectangle(batch, tex, x, y + 28, barWidth, 12, border);
            const int progress = barWidth * event.progress / event.goal;
            if (progress > 0)
                drawRectangle(batch, tex, x, y + 28, progress, 12, fill);
        }
        drawText(batch, event.label, static_cast<float>(width) * 0.5f,
                 static_cast<float>(y), fontColor);
    }
}

bool install() {
    if (g_installed) return true;
    const Refs& r = refs();
    if (!r.valid) return false;
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    const MethodInfo* method = main ? il2cpp::findMethodBySignature(
        main, il2cpp::parseSignature(
            "void DrawInterface_15_InvasionProgressBars()")) : nullptr;
    if (!method || !hook::install(method, drawHud, &g_original)) return false;
    g_installed = true;
    return true;
}

bool validName(const char* text, size_t max) {
    if (!text) return false;
    const size_t size = strnlen(text, max + 1);
    if (size == 0 || size > max) return false;
    for (size_t i = 0; i < size; ++i) {
        const char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    }
    return true;
}

}

void setNativeInvasionHudWorldState(bool ready, uint64_t session) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_session.load(std::memory_order_relaxed) != session ||
        (g_ready.load(std::memory_order_relaxed) && !ready))
        g_entries.clear();
    g_session.store(session, std::memory_order_release);
    g_ready.store(ready && session != 0, std::memory_order_release);
}

void pumpNativeInvasionHudOnGameThread() {
    if (g_ready.load(std::memory_order_acquire)) install();
}

}

extern "C" __attribute__((visibility("default")))
int bunny_invasion_hud_publish(int64_t session, const char* mod,
                              const char* invasion, int phase,
                              int progress, int goal) {
    using namespace bl::csharp;
    if (session <= 0 || !validName(mod, 128) ||
        !validName(invasion, 64) ||
        phase < 0 || phase > 2 || goal < 1 || goal > 1000000 ||
        progress < 0 || progress > goal ||
        (phase == 2 && progress != goal)) return 0;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_ready.load(std::memory_order_acquire) ||
        g_session.load(std::memory_order_acquire) != static_cast<uint64_t>(session))
        return 0;
    const Key key{mod, invasion};
    if (phase == 0) {
        g_entries.erase(key);
        return 1;
    }
    if (g_entries.size() >= 32 && !g_entries.contains(key)) return 0;
    std::string label = invasion;
    std::replace(label.begin(), label.end(), '-', ' ');
    if (label.size() > 0 && label[0] >= 'a' && label[0] <= 'z')
        label[0] = static_cast<char>(label[0] - 'a' + 'A');
    label += phase == 2 ? " - Complete!" :
        "  " + std::to_string(progress * 100 / goal) + "% (" +
        std::to_string(progress) + "/" + std::to_string(goal) + ")";
    g_entries[key] = {label, progress, goal, phase, 240};
    return 1;
}
