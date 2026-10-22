#include "content/backgrounds/SurfaceSpriteBatch.h"
#include <cmath>
#include <limits>

namespace bl::runtime::surface {
namespace {
using Property=il2cpp::CheckedProperty;
bool failure(std::string* error,const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    if(error)*error="[Native][ERROR][SurfaceBackground] API=SpriteBatch Stage="+std::string(stage)+" Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}
Il2CppClass* param(const MethodInfo* method,unsigned index) {
    auto& a=il2cpp::api();const auto* type=a.method_get_param(method,index);
    char* name=type?a.type_get_name(type):nullptr;
    if(!name)return nullptr;
    const bool indirect=name && (std::strchr(name,'&') || std::strchr(name,'*'));
    if(name)a.il2cpp_free(name);
    return !type || indirect?nullptr:a.class_from_il2cpp_type(type);
}
bool primitive(Il2CppClass* cls,const char* name,int bytes) {
    auto& a=il2cpp::api();uint32_t align=0;
    return cls && Property::name(cls)==name && a.class_is_valuetype(cls) && a.class_value_size(cls,&align)==bytes;
}
bool reference(Il2CppClass* cls,const char* name) {
    return cls && Property::name(cls)==name && !il2cpp::api().class_is_valuetype(cls);
}
bool returnsVoid(const MethodInfo* method) {
    auto& a=il2cpp::api();const auto* type=a.method_get_return_type(method);char* name=type?a.type_get_name(type):nullptr;
    const bool valid=name && !std::strchr(name,'&') && !std::strchr(name,'*') && Property::name(a.class_from_il2cpp_type(type))=="System.Void";
    if(name)a.il2cpp_free(name);
    return valid;
}
template<size_t N>bool offsets(Il2CppClass* type,const std::array<const char*,N>& names,const char* primitiveName,
                             std::array<size_t,N>* result,std::string* error) {
    auto& a=il2cpp::api();il2cpp::ValueLayout layout;
    if(!il2cpp::resolveValueLayout(type,&layout,error))return false;
    if(layout.size!=N*4)return failure(error,"BindValues","NativeValueSizeMismatch",std::to_string(N*4),std::to_string(layout.size));
    std::array<bool,N> used{};
    for(size_t i=0;i<N;++i) {
        auto* field=il2cpp::findField(type,names[i]);const auto* native=field?a.field_get_type(field):nullptr;
        auto* value=native?a.class_from_il2cpp_type(native):nullptr;size_t offset=0;
        char* typeName=native?a.type_get_name(native):nullptr;
        if(!typeName)return failure(error,"BindValues","NativeTypeNameUnavailable",names[i],"MissingTypeName");
        const bool indirect=typeName && (std::strchr(typeName,'&') || std::strchr(typeName,'*'));
        if(typeName)a.il2cpp_free(typeName);
        if(!field || (a.field_get_flags(field)&0x10u) || indirect || !primitive(value,primitiveName,4) ||
           !layout.offset(a.field_get_offset(field),4,&offset) || offset%4 || used[offset/4])
            return failure(error,"BindValues","NativeValueFieldMismatch",std::string(names[i])+":"+primitiveName,Property::name(value));
        used[offset/4]=true;(*result)[i]=offset;
    }
    return true;
}
bool invoke(const MethodInfo* method,Il2CppObject* owner,void** args,std::string* error,const char* stage) {
    auto& a=il2cpp::api();Il2CppObject* exception=nullptr;a.runtime_invoke(method,owner,args,&exception);
    if(!exception)return true;
    char detail[2048]{};
    if(a.format_exception)a.format_exception(exception,detail,sizeof(detail));
    detail[sizeof(detail)-1]=0;
    return failure(error,stage,"NativeDrawException","NoException",detail[0]?detail:Property::name(a.object_get_class(exception)));
}
}
bool SurfaceSpriteBatch::bind(Il2CppClass* batch,Il2CppClass* sky,std::string* error) {
    drawMethod=nullptr;skyMethod=nullptr;auto& a=il2cpp::api();
    std::string missing;const auto require=[&](auto fn,const char* name){if(!fn){if(!missing.empty())missing+=",";missing+=name;}};
    require(a.class_get_methods,"class_get_methods");require(a.method_get_name,"method_get_name");
    require(a.method_get_param,"method_get_param");require(a.method_get_param_count,"method_get_param_count");
    require(a.method_get_return_type,"method_get_return_type");require(a.method_get_flags,"method_get_flags");
    require(a.method_is_generic,"method_is_generic");require(a.class_from_il2cpp_type,"class_from_il2cpp_type");
    require(a.class_get_name,"class_get_name");require(a.class_get_namespace,"class_get_namespace");
    require(a.class_is_valuetype,"class_is_valuetype");require(a.class_value_size,"class_value_size");
    require(a.class_is_enum,"class_is_enum");require(a.class_enum_basetype,"class_enum_basetype");
    require(a.field_get_type,"field_get_type");require(a.field_get_flags,"field_get_flags");require(a.field_get_offset,"field_get_offset");
    require(a.class_get_fields,"class_get_fields");require(a.class_get_field_from_name,"class_get_field_from_name");
    require(a.class_get_parent,"class_get_parent");require(a.type_get_name,"type_get_name");require(a.il2cpp_free,"il2cpp_free");
    require(a.runtime_invoke,"runtime_invoke");require(a.object_get_class,"object_get_class");
    require(a.class_is_assignable_from,"class_is_assignable_from");require(a.gchandle_new,"gchandle_new");
    require(a.gchandle_free,"gchandle_free");require(a.gchandle_get_target,"gchandle_get_target");
    if(!missing.empty())return failure(error,"Bind","MetadataUnavailable","DrawingAndLayoutExports","Missing="+missing);
    if(!reference(batch,"Microsoft.Xna.Framework.Graphics.SpriteBatch") || !reference(sky,"Terraria.Graphics.Effects.SkyManager"))
        return failure(error,"Bind","NativeClassMismatch","SpriteBatchAndSkyManager",Property::name(batch)+","+Property::name(sky));
    const MethodInfo *draw=nullptr,*depth=nullptr;void* it=nullptr;
    while(auto* m=a.class_get_methods(batch,&it)) {
        const char* name=a.method_get_name(m);
        if(!name || std::strcmp(name,"Draw") || a.method_get_param_count(m)!=9 || (a.method_get_flags(m,nullptr)&(0x10u|0x400u)) || a.method_is_generic(m))continue;
        if(!returnsVoid(m) || !reference(param(m,0),"Microsoft.Xna.Framework.Graphics.Texture2D") ||
           Property::name(param(m,1))!="Microsoft.Xna.Framework.Vector2" || Property::name(param(m,2))!="System.Nullable`1" ||
           (Property::name(param(m,3))!="Microsoft.Xna.Framework.Graphics.Color" && Property::name(param(m,3))!="Microsoft.Xna.Framework.Color") ||
           !primitive(param(m,4),"System.Single",4) || param(m,1)!=param(m,5) || !primitive(param(m,6),"System.Single",4) ||
           Property::name(param(m,7))!="Microsoft.Xna.Framework.Graphics.SpriteEffects" || !primitive(param(m,8),"System.Single",4))continue;
        if(draw)return failure(error,"Bind","AmbiguousNativeOverload","OneExactDrawOverload","MultipleMatchingMethods");
        draw=m;
    }
    if(!draw)return failure(error,"Bind","NativeDrawMethodMissing","Instance:Void Draw(Texture2D,Vector2,Nullable<Rectangle>,Color,Single,Vector2,Single,SpriteEffects,Single)","NoMatchingOverload");
    auto* effects=param(draw,7);const auto* base=a.class_is_enum(effects)?a.class_enum_basetype(effects):nullptr;
    uint32_t align=0;
    if(!base || !primitive(a.class_from_il2cpp_type(base),"System.Int32",4) || !a.class_is_valuetype(effects) || a.class_value_size(effects,&align)!=4)
        return failure(error,"Bind","NativeEnumLayoutMismatch","SpriteEffects:System.Int32:4",Property::name(effects));
    auto* nullable=param(draw,2);auto* value=il2cpp::findField(nullable,"value");
    auto* rect=value?a.class_from_il2cpp_type(a.field_get_type(value)):nullptr;
    SurfaceSpriteBatch staged;
    if(Property::name(rect)!="Microsoft.Xna.Framework.Rectangle")
        return failure(error,"Bind","NativeNullableValueMismatch","Nullable<Microsoft.Xna.Framework.Rectangle>",Property::name(rect));
    if(!offsets(param(draw,1),std::array{"X","Y"},"System.Single",&staged.pointOffsets,error) ||
       !offsets(rect,std::array{"X","Y","Width","Height"},"System.Int32",&staged.rectangleOffsets,error) ||
       !staged.rectangle.bind(nullable,rect,error))return false;
    auto* colorClass=param(draw,3);
    for(const char* name:{"R","G","B","A"}) {
        auto* channel=il2cpp::findField(colorClass,name);
        const auto* native=channel?a.field_get_type(channel):nullptr;char* channelName=native?a.type_get_name(native):nullptr;
        const bool indirect=!channelName || std::strchr(channelName,'&') || std::strchr(channelName,'*');
        if(channelName)a.il2cpp_free(channelName);
        if(indirect)return failure(error,"BindColor","NativeColorChannelTypeMismatch",std::string(name)+":System.Byte",channel?"MissingTypeNameOrIndirectType":"MissingChannel");
    }
    if(!staged.colors.bind(colorClass,error)) {
        const auto detail=error?*error:"";
        return failure(error,"BindColor","NativeColorLayoutMismatch","FourDistinctByteChannels",Property::name(colorClass)+":"+detail);
    }
    it=nullptr;
    while(auto* m=a.class_get_methods(sky,&it)) {
        const char* name=a.method_get_name(m);
        if(!name || std::strcmp(name,"DrawToDepth") || a.method_get_param_count(m)!=2 ||
           (a.method_get_flags(m,nullptr)&(0x10u|0x400u)) || a.method_is_generic(m) || param(m,0)!=batch || !primitive(param(m,1),"System.Single",4) || !returnsVoid(m))continue;
        if(depth)return failure(error,"Bind","AmbiguousNativeOverload","OneExactDrawToDepthOverload","MultipleMatchingMethods");
        depth=m;
    }
    if(!depth)return failure(error,"Bind","NativeSkyMethodMissing","Instance:Void DrawToDepth(SpriteBatch,Single)","NoMatchingOverload");
    staged.batchClass=batch;staged.skyClass=sky;staged.textureClass=param(draw,0);
    staged.drawMethod=draw;staged.skyMethod=depth;*this=staged;return true;
}
bool SurfaceSpriteBatch::draw(Il2CppObject* batch,Il2CppObject* sky,Il2CppObject* texture,const DrawPlan& p,Color color,std::string* error) const {
    auto& a=il2cpp::api();
    if(!drawMethod || !skyMethod)return failure(error,"Draw","DrawingContractUnavailable","BoundDrawAndSkyContracts","Unbound");
    auto instance=[&](Il2CppObject* value,Il2CppClass* type){return value && a.class_is_assignable_from(type,a.object_get_class(value));};
    if(!instance(batch,batchClass) || !instance(texture,textureClass) || (p.drawSky && !instance(sky,skyClass)))
        return failure(error,"Draw","NativeObjectMismatch","CompatibleSpriteBatch,Texture2D,SkyManager","NullOrWrongClass");
    if(p.loops<0 || p.loops>32770 || p.width<=0 || p.width>32768 || p.height<=0 || p.height>32768 ||
       !std::isfinite(p.startX) || !std::isfinite(p.topY) || !std::isfinite(p.step) || !std::isfinite(p.scale) || p.scale<=0 ||
       std::abs(p.startX)>std::numeric_limits<float>::max() || std::abs(p.topY)>std::numeric_limits<float>::max() ||
       !std::isfinite(double(p.startX)+double(p.step)*std::max(0,p.loops-1)) ||
       std::abs(double(p.startX)+double(p.step)*std::max(0,p.loops-1))>std::numeric_limits<float>::max() ||
       (p.drawSky && (!std::isfinite(p.skyDepth) || p.skyDepth<=0)))
        return failure(error,"Draw","InvalidDrawPlan","FiniteBoundedDrawPlan","InvalidValueOrLoopCount");
    struct Roots {
        std::array<uint32_t,3> values{};
        ~Roots(){for(auto id:values)if(id)il2cpp::api().gchandle_free(id);}
    } roots;
    const std::array objects{batch,texture,p.drawSky?sky:nullptr};
    for(size_t i=0;i<objects.size();++i)if(objects[i]) {
        roots.values[i]=a.gchandle_new(objects[i],false);
        if(!roots.values[i])return failure(error,"Draw","GcRootAllocationFailed","RootedDrawObjects","RootIndex:"+std::to_string(i));
    }
    if(p.drawSky) {
        auto* rootedBatch=a.gchandle_get_target(roots.values[0]);float depth=p.skyDepth;void* args[]={rootedBatch,&depth};
        if(!invoke(skyMethod,a.gchandle_get_target(roots.values[2]),args,error,"DrawToDepth"))return false;
    }
    alignas(16) std::array<unsigned char,8> position{},origin{};
    alignas(16) std::array<unsigned char,16> rectangleValue{};
    const int32_t parts[]={0,0,p.width,p.height};
    for(size_t i=0;i<4;++i)std::memcpy(rectangleValue.data()+rectangleOffsets[i],parts+i,4);
    il2cpp::NullableLayout::Buffer source{};
    if(!rectangle.encode(true,rectangleValue.data(),rectangleValue.size(),&source,error))return false;
    uint32_t packed=colors.pack(color.r,color.g,color.b,color.a);float rotation=0,scale=p.scale,layerDepth=0;int32_t effects=0;
    const float y=float(p.topY);std::memcpy(position.data()+pointOffsets[1],&y,4);
    for(int k=0;k<p.loops;++k) {
        const float x=float(p.startX+p.step*k);
        std::memcpy(position.data()+pointOffsets[0],&x,4);
        void* args[]={a.gchandle_get_target(roots.values[1]),position.data(),source.data(),&packed,&rotation,origin.data(),&scale,&effects,&layerDepth};
        if(!invoke(drawMethod,a.gchandle_get_target(roots.values[0]),args,error,"DrawLayer"))return false;
    }
    return true;
}
}
