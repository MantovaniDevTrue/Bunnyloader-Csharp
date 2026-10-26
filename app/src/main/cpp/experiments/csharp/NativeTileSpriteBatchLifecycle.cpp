#include "experiments/csharp/NativeTileSpriteBatchLifecycle.h"
#include "experiments/csharp/NativeTileBatchPolicy.h"
#include "experiments/csharp/NativeTileSpriteBatch.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/ValueLayout.h"
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace bl::csharp::tile_sprite {
namespace {
using namespace il2cpp;

struct StaticObject {
    FieldInfo* field=nullptr;
    const MethodInfo* getter=nullptr;
    Il2CppClass* type=nullptr;
};
struct MatrixSource {
    FieldInfo* field=nullptr;
    const MethodInfo* getter=nullptr;
    Il2CppClass* type=nullptr;
    size_t bytes=0;
};
struct NativeBatch {
    const MethodInfo* begin=nullptr;
    const MethodInfo* end=nullptr;
    StaticObject blend,sampler,depth,raster;
    MatrixSource transform;
    size_t matrixLength=0,nullableValid=0,nullableMatrix=0;
    bool ready=false;
};
NativeBatch binding;

bool sameType(const Il2CppType* type,const char* expected) {
    auto& a=api();
    if(!type||!a.type_get_name||!a.il2cpp_free)return false;
    char* name=a.type_get_name(type);
    const bool ok=name&&std::strcmp(name,expected)==0;
    if(name)a.il2cpp_free(name);
    return ok;
}
bool method(const MethodInfo* m,const char* name,unsigned count,bool instance,const char* returns) {
    auto& a=api();
    return m&&std::strcmp(a.method_get_name(m),name)==0&&
        a.method_get_param_count(m)==count&&a.method_is_instance(m)==instance&&
        sameType(a.method_get_return_type(m),returns);
}
const MethodInfo* getter(Il2CppClass* owner,const char* name,const char* returns) {
    auto& a=api();
    const auto* m=owner?a.class_get_method_from_name(owner,name,0):nullptr;
    return method(m,name,0,false,returns)?m:nullptr;
}
bool objectSource(Il2CppClass* cls,const char* fieldName,const char* getterName,
                  const char* expected,StaticObject* output) {
    auto& a=api();
    if(!cls||!output)return false;
    auto* field=findField(cls,fieldName);
    if(field&&(a.field_get_flags(field)&0x10u)&&sameType(a.field_get_type(field),expected)) {
        output->field=field;
    } else {
        output->getter=getter(cls,getterName,expected);
        if(!output->getter)return false;
    }
    output->type=a.class_from_il2cpp_type(
        output->field?a.field_get_type(output->field):
        a.method_get_return_type(output->getter));
    return output->type&&!a.class_is_valuetype(output->type);
}
Il2CppObject* readObject(const StaticObject& source) {
    auto& a=api();
    Il2CppObject* value=nullptr;
    if(source.field) {
        a.field_static_get_value(source.field,&value);
    } else {
        Il2CppObject* exception=nullptr;
        value=a.runtime_invoke(source.getter,nullptr,nullptr,&exception);
        if(exception)return nullptr;
    }
    if(!value||!a.object_get_class)return nullptr;
    auto* actual=a.object_get_class(value);
    return actual==source.type||
        (a.class_is_assignable_from&&a.class_is_assignable_from(source.type,actual))
        ?value:nullptr;
}
bool matrixSource(Il2CppClass* cls,MatrixSource* out) {
    auto& a=api();
    constexpr const char* name="Microsoft.Xna.Framework.Matrix";
    auto* field=cls?findField(cls,"Transform"):nullptr;
    if(field&&(a.field_get_flags(field)&0x10u)&&sameType(a.field_get_type(field),name))
        out->field=field;
    else {
        out->getter=getter(cls,"get_Transform",name);
        if(!out->getter)return false;
    }
    out->type=a.class_from_il2cpp_type(out->field?a.field_get_type(out->field):
        a.method_get_return_type(out->getter));
    uint32_t align=0;
    if(!out->type||!a.class_is_valuetype(out->type)||
       a.class_value_size(out->type,&align)!=64)return false;
    out->bytes=64;
    return true;
}
bool readMatrix(const MatrixSource& source,std::array<uint8_t,64>* output) {
    auto& a=api();
    if(source.field)a.field_static_get_value(source.field,output->data());
    else {
        Il2CppObject* exception=nullptr;
        auto* box=a.runtime_invoke(source.getter,nullptr,nullptr,&exception);
        if(exception||!box||!a.object_get_class||a.object_get_class(box)!=source.type)return false;
        std::memcpy(output->data(),reinterpret_cast<const char*>(box)+sizeof(Il2CppObject),64);
    }
    for(size_t i=0;i<16;++i) {
        float value=0;
        std::memcpy(&value,output->data()+4*i,4);
        if(!std::isfinite(value))return false;
    }
    return true;
}
bool bind(std::string* error) {
    auto& a=api();
    if(!a.class_get_method_from_name||!a.method_get_return_type||
       !a.method_get_param_count||!a.method_get_param||!a.method_is_instance||
       !a.field_get_type||!a.field_get_flags||!a.field_static_get_value||
       !a.class_from_il2cpp_type||!a.class_value_size||!a.class_is_valuetype||
       !a.runtime_invoke||!a.object_get_class)return false;
    auto* cls=findClass({"Microsoft.Xna.Framework.Graphics","SpriteBatch",{}});
    auto* main=findClass({"Terraria","Main",{}});
    auto* blend=findClass({"Microsoft.Xna.Framework.Graphics","BlendState",{}});
    auto* depth=findClass({"Microsoft.Xna.Framework.Graphics","DepthStencilState",{}});
    if(!cls||!main||!blend||!depth)return false;
    const auto* begin=a.class_get_method_from_name(cls,"Begin",8);
    const auto* end=a.class_get_method_from_name(cls,"End",0);
    if(!method(begin,"Begin",8,true,"System.Void")||
       !method(end,"End",0,true,"System.Void")||
       !il2cpp::methodPointer(begin)||!il2cpp::methodPointer(end))return false;
    const char* args[]={
        "Microsoft.Xna.Framework.Graphics.SpriteSortMode",
        "Microsoft.Xna.Framework.Graphics.BlendState",
        "Microsoft.Xna.Framework.Graphics.SamplerState",
        "Microsoft.Xna.Framework.Graphics.DepthStencilState",
        "Microsoft.Xna.Framework.Graphics.RasterizerState",
        "Microsoft.Xna.Framework.Graphics.Effect"
    };
    for(unsigned i=0;i<6;++i)
        if(!sameType(a.method_get_param(begin,i),args[i]))return false;
    if(!sameType(a.method_get_param(begin,7),"System.Boolean"))return false;
    auto* nullable=a.class_from_il2cpp_type(a.method_get_param(begin,6));
    auto* value=nullable?findField(nullable,"value"):nullptr;
    auto* valid=nullable?findField(nullable,"hasValue"):nullptr;
    ValueLayout nlayout;
    if(!value||!valid||!sameType(a.field_get_type(value),"Microsoft.Xna.Framework.Matrix")||
       !sameType(a.field_get_type(valid),"System.Boolean")||
       !resolveValueLayout(nullable,&nlayout,error)||nlayout.size>128||
       !nlayout.offset(a.field_get_offset(value),64,&binding.nullableMatrix)||
       !nlayout.offset(a.field_get_offset(valid),1,&binding.nullableValid))return false;
    binding.matrixLength=nlayout.size;
    if(!objectSource(blend,"AlphaBlend","get_AlphaBlend",args[1],&binding.blend)||
       !objectSource(main,"DefaultSamplerState","get_DefaultSamplerState",args[2],&binding.sampler)||
       !objectSource(depth,"None","get_None",args[3],&binding.depth)||
       !objectSource(main,"Rasterizer","get_Rasterizer",args[4],&binding.raster)||
       !matrixSource(main,&binding.transform))return false;
    binding.begin=begin;
    binding.end=end;
    binding.ready=true;
    return true;
}
}

bool bindNativeTileBatchLifecycle(std::string* error) {
    if(binding.ready)return true;
    return bind(error);
}
bool beginNativeTileBatch(Il2CppObject* batch,bool* started) {
    if(started)*started=false;
    if(!started)return false;
    const auto action=chooseBatchAction(binding.ready,batch!=nullptr,
                                       nativeSpriteBatchIsOpen(batch));
    if(action==BatchAction::Reject)return false;
    if(action==BatchAction::Reuse)return true;
    auto& a=il2cpp::api();
    auto* blend=readObject(binding.blend),*sampler=readObject(binding.sampler);
    auto* depth=readObject(binding.depth),*raster=readObject(binding.raster);
    if(!blend||!sampler||!depth||!raster)return false;
    std::array<uint8_t,64> matrix{};
    if(!readMatrix(binding.transform,&matrix))return false;
    std::array<uint8_t,128> nullable{};
    if(binding.matrixLength>nullable.size()||
       binding.nullableMatrix+64>binding.matrixLength)return false;
    nullable[binding.nullableValid]=1;
    std::memcpy(nullable.data()+binding.nullableMatrix,matrix.data(),64);
    int32_t sort=0;
    uint8_t deferred=1;
    Il2CppObject* effect=nullptr;
    // BlendState/SamplerState/DepthStencilState/RasterizerState/Effect sao
    // classes: runtime_invoke recebe o objeto, nao o endereco da variavel.
    void* args[]={&sort,blend,sampler,depth,raster,effect,nullable.data(),&deferred};
    Il2CppObject* exception=nullptr;
    a.runtime_invoke(binding.begin,batch,args,&exception);
    if(exception||!nativeSpriteBatchIsOpen(batch))return false;
    *started=true;
    return true;
}
bool endNativeTileBatch(Il2CppObject* batch,bool started) {
    if(!started)return true;
    if(!binding.ready||!batch||!nativeSpriteBatchIsOpen(batch))return false;
    Il2CppObject* exception=nullptr;
    il2cpp::api().runtime_invoke(binding.end,batch,nullptr,&exception);
    return !exception&&!nativeSpriteBatchIsOpen(batch);
}
void resetNativeTileBatchLifecycle() {binding=NativeBatch{};}
}
