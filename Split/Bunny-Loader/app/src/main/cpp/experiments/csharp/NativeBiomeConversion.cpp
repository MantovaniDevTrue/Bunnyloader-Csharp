#include "experiments/csharp/NativeBiomeConversion.h"
#include "experiments/csharp/BiomeConversionEngine.h"
#include "content/tiles/TileAccess.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "core/Log.h"
#include <cstring>
#include <atomic>

namespace bl::csharp {
namespace {
BiomeConversionEngine engine;
std::atomic<bool> bindingsReady{false};
class World final : public BiomeConversionWorld {
    using Setter = void (*)(int32_t*, uint16_t, const MethodInfo*);
    using Frame = void (*)(int32_t,int32_t,bool,const MethodInfo*);
    Setter tileSetter=nullptr,wallSetter=nullptr;
    Frame tileFrame=nullptr,wallFrame=nullptr;
    const MethodInfo *mt=nullptr,*mw=nullptr,*ft=nullptr,*fw=nullptr;
    FieldInfo *menu=nullptr,*net=nullptr,*tileSolid=nullptr,*wallHouse=nullptr;
    bool linked=false;
    static bool typeIs(const Il2CppType* type,const char* name) {
        auto& a=il2cpp::api();
        if(!type || !a.type_get_name || !a.il2cpp_free) return false;
        char* found=a.type_get_name(type); bool ok=found && std::strcmp(found,name)==0;
        if(found)a.il2cpp_free(found);
        return ok;
    }
    static const MethodInfo* method(Il2CppClass* klass,const char* name,const std::vector<const char*>& args,bool isStatic) {
        auto& a=il2cpp::api();
        if(!klass || !a.class_get_methods || !a.method_get_param_count || !a.method_get_param ||
           !a.method_get_return_type || !a.method_get_flags || !a.method_get_name) return nullptr;
        void* it=nullptr;
        while(const MethodInfo* m=a.class_get_methods(klass,&it)) {
            if(std::strcmp(a.method_get_name(m),name)!=0 || a.method_get_param_count(m)!=args.size())continue;
            uint32_t flags=0; const uint32_t bits=a.method_get_flags(m,&flags);
            if(((bits & 0x10)!=0)!=isStatic || !typeIs(a.method_get_return_type(m),"System.Void"))continue;
            bool match=true;for(size_t i=0;i<args.size();++i) match=match && typeIs(a.method_get_param(m,i),args[i]);
            if(match && il2cpp::methodPointer(m))return m;
        }
        return nullptr;
    }
public:
    bool resolve() {
        if(linked)return true;
        auto& a=il2cpp::api();
        if(!a.field_static_get_value || !a.class_value_size || !a.field_get_type || !a.field_get_flags)return false;
        auto tile=il2cpp::findClass({"Terraria","Tile",{}});
        auto world=il2cpp::findClass({"Terraria","WorldGen",{}});
        auto main=il2cpp::findClass({"Terraria","Main",{}});
        if(!tile || !world || !main)return false;
        uint32_t align=0;if(a.class_value_size(tile,&align)!=4)return false;
        mt=method(tile,"set_type",{"System.UInt16"},false); mw=method(tile,"set_wall",{"System.UInt16"},false);
        ft=method(world,"SquareTileFrame",{"System.Int32","System.Int32","System.Boolean"},true);
        fw=method(world,"SquareWallFrame",{"System.Int32","System.Int32","System.Boolean"},true);
        menu=il2cpp::findField(main,"gameMenu"); net=il2cpp::findField(main,"netMode");
        tileSolid=il2cpp::findField(main,"tileSolid");wallHouse=il2cpp::findField(main,"wallHouse");
        if(!mt || !mw || !ft || !fw || !menu || !net || !tileSolid || !wallHouse)return false;
        for(auto* field:{menu,net,tileSolid,wallHouse})if(!(a.field_get_flags(field)&0x10))return false;
        if(!typeIs(a.field_get_type(menu),"System.Boolean") || !typeIs(a.field_get_type(net),"System.Int32") ||
           !typeIs(a.field_get_type(tileSolid),"System.Boolean[]") || !typeIs(a.field_get_type(wallHouse),"System.Boolean[]"))return false;
        tileSetter=reinterpret_cast<Setter>(il2cpp::methodPointer(mt));wallSetter=reinterpret_cast<Setter>(il2cpp::methodPointer(mw));
        tileFrame=reinterpret_cast<Frame>(il2cpp::methodPointer(ft));wallFrame=reinterpret_cast<Frame>(il2cpp::methodPointer(fw));
        linked=true;return true;
    }
public:
    bool ready() override {
        if(!resolve())return false;
        uint8_t isMenu=1;int mode=-1;auto& a=il2cpp::api();
        a.field_static_get_value(menu,&isMenu);a.field_static_get_value(net,&mode);
        return !isMenu && mode==0;
    }
    bool read(int x,int y,int* tile,int* wall) override {
        runtime::TileArrays arrays;
        if(!runtime::tileArrays(&arrays) || x<1 || y<1 || x>=arrays.width-1 || y>=arrays.height-1)return false;
        *tile=runtime::tileTypeAt(x,y);*wall=runtime::wallTypeAt(x,y);return true;
    }
    bool validTarget(int type,bool wall) override {
        Il2CppArray* values=nullptr;il2cpp::api().field_static_get_value(wall?wallHouse:tileSolid,&values);
        return values && type>=0 && static_cast<uintptr_t>(type)<values->length;
    }
    void write(int x,int y,int tile,int wall) override {
        runtime::TileArrays arrays;
        if(!runtime::tileArrays(&arrays))return;
        int32_t offset=y*arrays.width+x;
        // Tile definitions are INTERNED on Mobile. Only these checked instance setters
        // perform copy-on-write; assigning arrays.type[lookup[offset]] would corrupt peers.
        if(tile>=0)tileSetter(&offset,static_cast<uint16_t>(tile),mt);
        if(wall>=0)wallSetter(&offset,static_cast<uint16_t>(wall),mw);
        if(tile>=0)tileFrame(x,y,true,ft);
        if(wall>=0)wallFrame(x,y,true,fw);
    }
};
World world;
}
bool nativeBiomeConversionReady(){return !engine.size() || bindingsReady.load(std::memory_order_acquire);}
void resetBiomeConversionWorld(){engine.world(false);}
void beginBiomeConversionWorld(){engine.world(true);}
void pumpBiomeConversionsOnGameThread(){
    if(!engine.size())return;
    if(!world.resolve()) {
        static bool reported=false;
        if(!reported){reported=true;BL_ERROR("[CSharp][BiomeConversion] Tile/setter/frame ABI unavailable; world gate held, conversion writes disabled");}
        return;
    }
    bindingsReady.store(true,std::memory_order_release);
    engine.pump(world);
}
}
extern "C" __attribute__((visibility("default"))) int bunny_biome_conversion_publish(const char* owner,const char* wire) {
    if(!owner || !wire || strnlen(owner,161)>160 || strnlen(wire,256*1024+1)>256*1024)return 0;
    return bl::csharp::engine.publish(owner,wire)?1:0;
}
extern "C" __attribute__((visibility("default"))) int bunny_biome_conversion_request(const char* owner,int type,int x,int y,int size,int tiles,int walls) {
    if(!owner || strnlen(owner,161)>160 || (tiles!=0 && tiles!=1) || (walls!=0 && walls!=1))return 0;
    return bl::csharp::engine.request(owner,type,x,y,size,tiles!=0,walls!=0)?1:0;
}
