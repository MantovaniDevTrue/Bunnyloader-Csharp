#include "experiments/csharp/NativeTileSpriteBatch.h"
#include "experiments/csharp/NativeColorLayout.h"
#include "content/common/ContentAssets.h"
#include "content/tiles/ModTiles.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace bl::csharp::tile_sprite {
namespace {
struct Vec { float x = 0; float y = 0; };
struct Bounds {
    size_t sourceValue = 0;
    size_t sourceValid = 0;
    size_t x = 0;
    size_t y = 0;
    size_t width = 0;
    size_t height = 0;
    size_t bytes = 0;
};
struct Binding {
    const MethodInfo* draw = nullptr;
    FieldInfo* tileAssets = nullptr;
    size_t beginOffset = 0;
    Bounds source;
    NativeColorLayout color;
    bool ready = false;
};
Binding g;

bool hasName(const Il2CppType* type,const char* expected) {
    auto& a = il2cpp::api();
    if (!type || !a.type_get_name || !a.il2cpp_free) return false;
    char* name = a.type_get_name(type);
    const bool ok = name && std::strcmp(name,expected) == 0;
    if (name) a.il2cpp_free(name);
    return ok;
}

bool bindRectangle(Il2CppClass* cls,Bounds* out,std::string* error) {
    using namespace il2cpp;
    if (!cls) return false;
    ValueLayout rectangle;
    if (!resolveValueLayout(cls,&rectangle,error) || rectangle.size != 16) return false;
    const char* fields[] = {"X","Y","Width","Height"};
    size_t* offsets[] = {&out->x,&out->y,&out->width,&out->height};
    for (int index = 0; index < 4; ++index) {
        FieldInfo* f = findField(cls,fields[index]);
        if (!f || !hasName(api().field_get_type(f),"System.Int32") ||
            !rectangle.offset(api().field_get_offset(f),4,offsets[index]) ||
            *offsets[index] != static_cast<size_t>(index*4)) return false;
    }
    return true;
}

bool bind(std::string* error) {
    using namespace il2cpp;
    auto& a = api();
    if (!a.runtime_invoke || !a.field_static_get_value || !a.class_get_methods ||
        !a.field_get_flags || !a.field_get_offset ||
        !a.method_get_param || !a.method_get_param_count ||
        !a.class_from_il2cpp_type) return false;
    auto* batch = findClass({"Microsoft.Xna.Framework.Graphics","SpriteBatch",{}});
    auto* main = findClass({"Terraria","Main",{}});
    auto* textures = findClass({"Terraria.GameContent","TextureAssets",{}});
    if (!batch || !main || !textures) return false;
    auto* began = findField(batch,"_beginCalled");
    auto* assets = findField(textures,"Tile");
    if (!began || !hasName(a.field_get_type(began),"System.Boolean") ||
        (a.field_get_flags(began)&0x10u) ||
        a.field_get_offset(began)<sizeof(Il2CppObject) ||
        !assets || !(a.field_get_flags(assets)&0x10u)) return false;
    const MethodInfo* draw = nullptr;
    void* it = nullptr;
    while (auto* m = a.class_get_methods(batch,&it)) {
        if (std::strcmp(a.method_get_name(m),"Draw") != 0 ||
            a.method_get_param_count(m) != 9 ||
            !a.method_is_instance(m) ||
            !hasName(a.method_get_param(m,0),"Microsoft.Xna.Framework.Graphics.Texture2D") ||
            !hasName(a.method_get_param(m,1),"Microsoft.Xna.Framework.Vector2") ||
            !hasName(a.method_get_param(m,3),"Microsoft.Xna.Framework.Color") ||
            !hasName(a.method_get_param(m,4),"System.Single") ||
            !hasName(a.method_get_param(m,5),"Microsoft.Xna.Framework.Vector2") ||
            !hasName(a.method_get_param(m,6),"System.Single") ||
            !hasName(a.method_get_param(m,7),"Microsoft.Xna.Framework.Graphics.SpriteEffects") ||
            !hasName(a.method_get_param(m,8),"System.Single")) continue;
        auto* nullable = a.class_from_il2cpp_type(a.method_get_param(m,2));
        auto* field = nullable ? findField(nullable,"value") : nullptr;
        if (!field) continue;
        auto* rectangle = a.class_from_il2cpp_type(a.field_get_type(field));
        Bounds source{};
        ValueLayout layout;
        if (!resolveValueLayout(nullable,&layout,error) || layout.size>64 ||
            !bindRectangle(rectangle,&source,error) ||
            !layout.offset(a.field_get_offset(field),16,&source.sourceValue)) continue;
        auto* validField = findField(nullable,"hasValue");
        if (!validField || !hasName(a.field_get_type(validField),"System.Boolean") ||
            !layout.offset(a.field_get_offset(validField),1,&source.sourceValid) ||
            source.sourceValue+16>layout.size) continue;
        auto* color = a.class_from_il2cpp_type(a.method_get_param(m,3));
        NativeColorLayout colors;
        if (!colors.bind(color,error)) continue;
        source.bytes = layout.size;
        draw = m;
        g.source = source;
        g.color = colors;
        break;
    }
    if (!draw) {
        if (error) *error = "Stage=BindSpriteBatch Reason=DrawNineArgumentSpriteEffectsUnavailable";
        return false;
    }
    g.draw = draw;
    g.tileAssets = assets;
    g.beginOffset = a.field_get_offset(began);
    g.ready = true;
    return true;
}
}

bool bindNativeSpriteBatch(std::string* error) {
    if (g.ready) return true;
    return bind(error);
}

bool nativeSpriteBatchIsOpen(Il2CppObject* batch) {
    if (!g.ready || !batch) return false;
    return *reinterpret_cast<const uint8_t*>(
        reinterpret_cast<const char*>(batch)+g.beginOffset) != 0;
}

bool drawNativeTileSprite(Il2CppObject* batch,const Sprite& command,
                          float screenX,float screenY) {
    using namespace il2cpp;
    if (!g.ready || !batch || !valid(command) ||
        !std::isfinite(screenX) || !std::isfinite(screenY) ||
        screenX < -2000000.f || screenY < -2000000.f ||
        screenX > 2000000.f || screenY > 2000000.f) return false;
    auto& a = api();
    if (!nativeSpriteBatchIsOpen(batch)) return false;
    Il2CppObject* asset=nullptr;
    if(command.textureLayer==0) {
        Il2CppArray* tiles=nullptr;
        a.field_static_get_value(g.tileAssets,&tiles);
        if (!tiles || static_cast<uintptr_t>(command.type)>=tiles->length) return false;
        asset=static_cast<Il2CppObject**>(arrayData(tiles))[command.type];
    } else asset=runtime::modTileOverlayAsset(command.type,command.textureLayer);
    auto* texture=asset?runtime::content::textureAssetValue(asset):nullptr;
    if (!texture) return false;
    Vec pos{static_cast<float>(command.x*16 + command.offsetX)-std::floor(screenX),
            static_cast<float>(command.y*16 + command.offsetY)-std::floor(screenY)};
    std::array<uint8_t,64> source{};
    if (g.source.bytes>source.size()) return false;
    source[g.source.sourceValid]=1;
    int rect[]={command.frameX,command.frameY,command.width,command.height};
    size_t offsets[]={g.source.x,g.source.y,g.source.width,g.source.height};
    for (int index=0;index<4;++index)
        std::memcpy(source.data()+g.source.sourceValue+offsets[index],
                    &rect[index],sizeof(int));
    uint32_t color=g.color.fromArgb(command.argb);
    float rotation=0.f,scale=1.f,depth=0.f;
    Vec origin{};
    int32_t effect=command.effects;
    void* arguments[]={&texture,&pos,source.data(),&color,
                       &rotation,&origin,&scale,&effect,&depth};
    Il2CppObject* exception=nullptr;
    a.runtime_invoke(g.draw,batch,arguments,&exception);
    return exception==nullptr;
}

void resetNativeSpriteBatch() {
    g=Binding{};
}
}
