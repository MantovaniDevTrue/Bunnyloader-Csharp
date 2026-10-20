#pragma once

bool applyNativeSaplingShape(const std::string& mod,int type,const ManagedTileEntry& entry) {
    if(entry.shape!=4 || entry.saplingStyleRange<1 || entry.saplingStyleRange>12 ||
       (entry.saplingGroundTile.empty() && entry.saplingVanillaGroundTile<0) ||
       (entry.flags&(131072|262144|524288))!=(131072|262144|524288)) return false;
    const int ground=entry.saplingGroundTile.empty()?-1:
        runtime::modTileTypeByKey(mod+"/"+entry.saplingGroundTile);
    if(!entry.saplingGroundTile.empty() &&
       (ground<runtime::kVanillaTileCount || ground>=65536))return false;
    if(entry.saplingVanillaGroundTile< -1 ||
       entry.saplingVanillaGroundTile>=runtime::kVanillaTileCount)return false;
    auto& a=il2cpp::api();
    if(!a.runtime_invoke||!a.array_new||!a.value_box||!a.class_value_size||
       !a.class_is_valuetype||!a.method_is_instance||!a.method_get_param_count||
       !a.method_get_param||!a.method_get_return_type||!a.object_get_class||
       !a.type_get_name||!a.il2cpp_free)return false;
    auto* cls=il2cpp::findClass({"Terraria.ObjectData","TileObjectData",{}});
    auto* tile=tileStaticObject(cls,"newTile");
    auto* integer=il2cpp::findClass({"System","Int32",{}});
    auto* point=il2cpp::findClass({"Terraria.DataStructures","Point16",{}});
    auto* anchor=il2cpp::findClass({"Terraria.DataStructures","AnchorData",{}});
    if(!cls||!tile||!integer||!point||!anchor)return false;
    auto* tileClass=a.object_get_class(tile);
    const auto typeIs=[&](const Il2CppType* t,const char* wanted) {
        if(!t)return false;
        char* n=a.type_get_name(t);
        const bool ok=n&&std::strcmp(n,wanted)==0;
        if(n)a.il2cpp_free(n);
        return ok;
    };
    const auto* add=a.class_get_method_from_name(cls,"addTile",1);
    if(!add||a.method_is_instance(add)||a.method_get_param_count(add)!=1||
       !typeIs(a.method_get_param(add,0),"System.Int32")||
       !typeIs(a.method_get_return_type(add),"System.Void")||
       !il2cpp::methodPointer(add))return false;
    uint32_t alignment=0;
    if(!a.class_is_valuetype(point)||!a.class_is_valuetype(anchor)||
       a.class_value_size(point,&alignment)!=4 ||
       a.class_value_size(anchor,&alignment)!=12)return false;
    const auto* pointCtor=a.class_get_method_from_name(point,".ctor",2);
    const auto* anchorCtor=a.class_get_method_from_name(anchor,".ctor",3);
    if(!pointCtor||!anchorCtor||!a.method_is_instance(pointCtor)||
       !a.method_is_instance(anchorCtor)||a.method_get_param_count(pointCtor)!=2||
       a.method_get_param_count(anchorCtor)!=3||
       !typeIs(a.method_get_return_type(pointCtor),"System.Void")||
       !typeIs(a.method_get_return_type(anchorCtor),"System.Void")||
       !typeIs(a.method_get_param(pointCtor,0),"System.Int32")||
       !typeIs(a.method_get_param(pointCtor,1),"System.Int32")||
       !typeIs(a.method_get_param(anchorCtor,0),"Terraria.Enums.AnchorType")||
       !typeIs(a.method_get_param(anchorCtor,1),"System.Int32")||
       !typeIs(a.method_get_param(anchorCtor,2),"System.Int32")||
       !il2cpp::methodPointer(pointCtor)||
       !il2cpp::methodPointer(anchorCtor))return false;
    struct Slot {const MethodInfo* setter=nullptr;int offset=-1;size_t size=0;};
    const auto bind=[&](const char* name,const char* type,size_t size,Slot* out) {
        const std::string setter=std::string("set_")+name;
        const auto* method=a.class_get_method_from_name(tileClass,setter.c_str(),1);
        if(method) {
            if(!il2cpp::methodPointer(method)||!a.method_is_instance(method)||
               a.method_get_param_count(method)!=1||
               !typeIs(a.method_get_param(method,0),type)||
               !typeIs(a.method_get_return_type(method),"System.Void"))return false;
            *out={method,-1,size};return true;
        }
        FieldInfo* f=nullptr;int offset=-1;std::string error;
        if(!il2cpp::nativeFieldContract(tileClass,name,type,false,size,
                                         &f,&offset,&error))return false;
        *out={nullptr,offset,size};return true;
    };
    Slot width{},height{},origin{},bottom{},custom{},heights{},coordWidth{},
         padding{},anchors{},horizontal{},flip{},lava{},random{},multiplier{};
    if(!bind("Width","System.Int32",4,&width)||
       !bind("Height","System.Int32",4,&height)||
       !bind("Origin","Terraria.DataStructures.Point16",4,&origin)||
       !bind("AnchorBottom","Terraria.DataStructures.AnchorData",12,&bottom)||
       !bind("UsesCustomCanPlace","System.Boolean",1,&custom)||
       !bind("CoordinateHeights","System.Int32[]",sizeof(void*),&heights)||
       !bind("CoordinateWidth","System.Int32",4,&coordWidth)||
       !bind("CoordinatePadding","System.Int32",4,&padding)||
       !bind("AnchorValidTiles","System.Int32[]",sizeof(void*),&anchors)||
       !bind("StyleHorizontal","System.Boolean",1,&horizontal)||
       !bind("DrawFlipHorizontal","System.Boolean",1,&flip)||
       !bind("LavaDeath","System.Boolean",1,&lava)||
       !bind("RandomStyleRange","System.Int32",4,&random)||
       !bind("StyleMultiplier","System.Int32",4,&multiplier))return false;
    auto* coordinateHeights=a.array_new(integer,2);
    const uintptr_t count=(ground>=0?1:0)+(entry.saplingVanillaGroundTile>=0?1:0);
    auto* validAnchors=a.array_new(integer,count);
    if(!coordinateHeights||!validAnchors)return false;
    auto* coordinates=static_cast<int32_t*>(arrayData(coordinateHeights));
    coordinates[0]=16;coordinates[1]=18;
    auto* values=static_cast<int32_t*>(arrayData(validAnchors));
    uintptr_t pos=0;
    if(ground>=0)values[pos++]=ground;
    if(entry.saplingVanillaGroundTile>=0)values[pos++]=entry.saplingVanillaGroundTile;
    std::array<uint8_t,16> pointBytes{},anchorBytes{};
    auto* boxedPoint=a.value_box(point,pointBytes.data());
    auto* boxedAnchor=a.value_box(anchor,anchorBytes.data());
    if(!boxedPoint||!boxedAnchor)return false;
    int32_t px=0,py=1,anchorType=1,anchorCount=1,anchorStart=0;
    void* pointArgs[]={&px,&py};
    void* anchorArgs[]={&anchorType,&anchorCount,&anchorStart};
    Il2CppObject* error=nullptr;
    a.runtime_invoke(pointCtor,boxedPoint,pointArgs,&error);
    if(error)return false;
    a.runtime_invoke(anchorCtor,boxedAnchor,anchorArgs,&error);
    if(error)return false;
    const auto set=[&](const Slot& slot,const void* value) {
        if(slot.setter) {
            void* args[]={const_cast<void*>(value)};
            Il2CppObject* exception=nullptr;
            a.runtime_invoke(slot.setter,tile,args,&exception);
            return exception==nullptr;
        }
        if(slot.offset<0)return false;
        std::memcpy(reinterpret_cast<char*>(tile)+slot.offset,value,slot.size);
        return true;
    };
    int32_t w=1,h=2,coordW=16,pad=2,range=entry.saplingStyleRange;
    bool enabled=true;
    if(!set(width,&w)||!set(height,&h)||
       !set(origin,reinterpret_cast<char*>(boxedPoint)+sizeof(Il2CppObject))||
       !set(bottom,reinterpret_cast<char*>(boxedAnchor)+sizeof(Il2CppObject))||
       !set(custom,&enabled)||!set(heights,coordinateHeights)||
       !set(coordWidth,&coordW)||!set(padding,&pad)||!set(anchors,validAnchors)||
       !set(horizontal,&enabled)||!set(flip,&enabled)||!set(lava,&enabled)||
       !set(random,&range)||!set(multiplier,&range))return false;
    int32_t id=type;
    void* args[]={&id};
    a.runtime_invoke(add,nullptr,args,&error);
    return error==nullptr;
}
