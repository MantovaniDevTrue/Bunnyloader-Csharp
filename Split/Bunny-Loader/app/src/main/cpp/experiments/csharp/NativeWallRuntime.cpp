#include "experiments/csharp/NativeWallRuntime.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/walls/ModWalls.h"
#include "content/tiles/ModTileMap.h"
#include "content/tiles/TileAccess.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include "il2cpp/GuardedCall.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>

namespace bl::csharp::walls {
namespace {
struct Policy {Definition definition;int blend=-1,dust=-1;std::map<int,int> frameDust;};
using Table=std::map<int,Policy>;
std::shared_ptr<const Table> table=std::make_shared<const Table>();
std::mutex mutex;
std::map<int,Policy> queued;
std::set<int> failed;
std::atomic<uint32_t> callbackErrors{0};
struct Array {FieldInfo* field=nullptr;const char* element="";int bytes=0;};
struct Refs {
    Il2CppClass* tile=nullptr;Il2CppClass* point=nullptr;Il2CppClass* lookupRowClass=nullptr;
    FieldInfo *house=nullptr,*blend=nullptr,*day=nullptr,*lookup=nullptr;
    Array frames,counters;
    const MethodInfo *number=nullptr,*frameX=nullptr,*frameY=nullptr,*setNumber=nullptr,*setX=nullptr,*setY=nullptr,*newDust=nullptr,*addLight=nullptr;
    int pointBytes=0,coordinateBytes=0;
} refs;
using Kill=void(*)(int32_t,int32_t,bool,const MethodInfo*);
using Dust=void(*)(int32_t,int32_t,uint32_t,const MethodInfo*);
using Drop=int32_t(*)(uint32_t,const MethodInfo*);
using Animate=void(*)(const MethodInfo*);
using Frame=void(*)(int32_t,int32_t,bool,const MethodInfo*);
using Area=void(*)(int32_t,int32_t,int32_t,int32_t,const MethodInfo*);
Kill originalKill=nullptr;Dust originalDust=nullptr;Drop originalDrop=nullptr;
Animate originalAnimate=nullptr;Frame originalFrame=nullptr;Area originalArea=nullptr;
struct DustScope {int type=-1,left=-1;};thread_local DustScope dustScope;
std::atomic<uint32_t> randomState{0x6d2b79f5u};
uint32_t randomValue(){auto value=randomState.fetch_add(0x9e3779b9u);value^=value>>16;value*=0x85ebca6bu;value^=value>>13;return value;}
void diagnostic(const Definition& d,const char* stage,const std::string& detail) {
    if(std::strcmp(stage,"CreateDust")==0||std::strcmp(stage,"KillWall")==0) {
        const auto n=callbackErrors.fetch_add(1);if(n>=3&&(n&(n-1))!=0)return;
    }
    BL_ERROR("[CSharp][ERROR][Wall] Mod=%s Class=%s File=%s ContentId=%d Stage=%s Result=Rejected Detail=%s",d.mod.c_str(),d.name.c_str(),d.source.c_str(),d.type,stage,detail.c_str());
}
const MethodInfo* method(Il2CppClass* cls,const char* signature,bool instance,std::string* error) {
    auto* m=cls?il2cpp::findMethodBySignature(cls,il2cpp::parseSignature(signature)):nullptr;
    if(!m||!il2cpp::api().method_is_instance||il2cpp::api().method_is_instance(m)!=instance||!il2cpp::methodPointer(m)) {
        *error="Expected="+std::string(instance?"Instance:":"Static:")+signature+" Reason=NativeMethodContractMismatch";return nullptr;
    }
    return m;
}
Il2CppArray* array(FieldInfo* f){Il2CppArray* result=nullptr;if(f)il2cpp::api().field_static_get_value(f,&result);return result;}
bool staticArray(Il2CppClass* cls,const char* name,const char* element,int bytes,Array* out,std::string* error) {
    const auto signature="System."+std::string(element)+"[]";
    FieldInfo* field=nullptr;if(!il2cpp::nativeFieldContract(cls,name,signature.c_str(),true,sizeof(void*),&field,nullptr,error))return false;
    *out={field,element,bytes};return true;
}
bool frameArray(Il2CppClass* cls,const char* name,Array* out,std::string* error) {
    for(const auto& candidate:std::array<std::pair<const char*,int>,3>{{{"Byte",1},{"UInt16",2},{"Int32",4}}})
        if(staticArray(cls,name,candidate.first,candidate.second,out,error))return true;
    *error="Field="+std::string(name)+" Expected=Static:Byte[]/UInt16[]/Int32[] Detail="+*error;return false;
}
bool validArray(const Array& spec,Il2CppArray* value,int type) {
    return il2cpp::nativeArrayContract(value,spec.element,spec.bytes,65536)&&type>=0&&static_cast<uintptr_t>(type)<value->length;
}
int read(const Array& spec,Il2CppArray* value,int type) {uint32_t result=0;std::memcpy(&result,static_cast<char*>(arrayData(value))+size_t(type)*spec.bytes,spec.bytes);return static_cast<int>(result);}
void write(const Array& spec,Il2CppArray* value,int type,int number){const auto n=static_cast<uint32_t>(number);std::memcpy(static_cast<char*>(arrayData(value))+size_t(type)*spec.bytes,&n,spec.bytes);}
bool tileMethod(const MethodInfo* m,int tileArgument) {
    auto& a=il2cpp::api();if(!m||!a.method_get_param||!a.class_value_size)return false;
    auto* type=a.class_from_il2cpp_type(a.method_get_param(m,tileArgument));uint32_t align=0;
    return type==refs.tile&&a.class_value_size(type,&align)==4&&align==4;
}
int numberAt(uint32_t cell){int32_t offset=static_cast<int32_t>(cell);return reinterpret_cast<uint8_t(*)(int32_t*,const MethodInfo*)>(il2cpp::methodPointer(refs.number))(&offset,refs.number);}
int coordinate(uint32_t cell,const MethodInfo* m){int32_t offset=static_cast<int32_t>(cell);return reinterpret_cast<int32_t(*)(int32_t*,const MethodInfo*)>(il2cpp::methodPointer(m))(&offset,m);}
void setCoordinate(uint32_t cell,int value,const MethodInfo* m){int32_t offset=static_cast<int32_t>(cell);reinterpret_cast<void(*)(int32_t*,int32_t,const MethodInfo*)>(il2cpp::methodPointer(m))(&offset,value,m);}
void setNumber(uint32_t cell,int value){int32_t offset=static_cast<int32_t>(cell);reinterpret_cast<void(*)(int32_t*,uint8_t,const MethodInfo*)>(il2cpp::methodPointer(refs.setNumber))(&offset,static_cast<uint8_t>(value),refs.setNumber);}
void kill(int32_t x,int32_t y,bool fail,const MethodInfo* m) {
    const auto view=std::atomic_load(&table);auto found=view->find(runtime::wallTypeAt(x,y));
    if(found==view->end()){if(originalKill)originalKill(x,y,fail,m);return;}
    const auto previous=dustScope;dustScope={found->first,fail?found->second.definition.failDust:found->second.definition.normalDust};
    il2cpp::Caught caught;auto run=[&]{if(originalKill)originalKill(x,y,fail,m);};
    const bool ok=il2cpp::guarded(run,&caught);dustScope=previous;
    if(!ok)diagnostic(found->second.definition,"KillWall",il2cpp::describeCaught(caught));
}
void dust(int32_t x,int32_t y,uint32_t cell,const MethodInfo* m) {
    const auto view=std::atomic_load(&table);auto found=view->find(runtime::wallTypeAtOffset(static_cast<int32_t>(cell)));
    if(found==view->end()){if(originalDust)originalDust(x,y,cell,m);return;}
    if(dustScope.type==found->first){if(dustScope.left==0)return;if(dustScope.left>0)--dustScope.left;}
    int id=found->second.dust;auto conditional=found->second.frameDust.find(numberAt(cell));if(conditional!=found->second.frameDust.end())id=conditional->second;
    if(id<0){if(originalDust)originalDust(x,y,cell,m);return;}
    float pos[]={float(x)*16,float(y)*16},speed=0,scale=1;int width=16,height=16,alpha=0;uint32_t color=0xffffffff;
    void* args[]={pos,&width,&height,&id,&speed,&speed,&alpha,&color,&scale};Il2CppObject* exception=nullptr;
    il2cpp::api().runtime_invoke(refs.newDust,nullptr,args,&exception);
    if(exception)diagnostic(found->second.definition,"CreateDust","Reason=NativeDustException Exception="+il2cpp::describeException(exception));
}
int32_t drop(uint32_t cell,const MethodInfo* m) {
    const auto view=std::atomic_load(&table);auto found=view->find(runtime::wallTypeAtOffset(static_cast<int32_t>(cell)));
    if(found==view->end())return originalDrop?originalDrop(cell,m):0;
    const int item=basicItemForWall(found->second.definition.mod+"/"+found->second.definition.name);
    return std::max(0,item);
}
void animate(const MethodInfo* m) {
    if(originalAnimate)originalAnimate(m);
    const auto view=std::atomic_load(&table);auto* frames=array(refs.frames.field);auto* counters=array(refs.counters.field);
    for(const auto& [type,p]:*view)if(p.definition.frames>1&&validArray(refs.frames,frames,type)&&validArray(refs.counters,counters,type)) {
        int count=read(refs.counters,counters,type),frame=read(refs.frames,frames,type);
        if(count<0||count>255||frame<0||frame>255)continue;
        frame=nextFrame(frame,&count,p.definition.frames,p.definition.ticks);
        write(refs.frames,frames,type,frame);write(refs.counters,counters,type,count);
    }
}
bool lookupRow(Il2CppArray* row){auto& a=il2cpp::api();return row&&row->obj.klass&&row->length<=256&&a.class_get_rank(row->obj.klass)==1&&a.class_get_element_class(row->obj.klass)==refs.point;}
std::pair<int,int> point(Il2CppArray* row,size_t index) {
    const char* data=static_cast<const char*>(arrayData(row))+index*refs.pointBytes;
    if(refs.coordinateBytes==2){int16_t x=0,y=0;std::memcpy(&x,data,2);std::memcpy(&y,data+2,2);return {x,y};}
    int32_t x=0,y=0;std::memcpy(&x,data,4);std::memcpy(&y,data+4,4);return {x,y};
}
void frame(int32_t x,int32_t y,bool reset,const MethodInfo* m) {
    if(originalFrame)originalFrame(x,y,reset,m);
    if(!reset)return;
    const auto view=std::atomic_load(&table);auto found=view->find(runtime::wallTypeAt(x,y));
    if(found==view->end()||found->second.definition.choices.empty())return;
    runtime::TileArrays world{};if(!runtime::tileArrays(&world)||x<0||y<0||x>=world.width||y>=world.height)return;
    const auto offset=static_cast<uint64_t>(y)*world.width+x;if(offset>INT32_MAX)return;
    const auto cell=static_cast<uint32_t>(offset);const int n=numberAt(cell);const auto& d=found->second.definition;
    if(n!=d.from||double(randomValue())/4294967296.0>=d.chance)return;
    auto* lookup=array(refs.lookup);auto& a=il2cpp::api();
    if(!lookup||!lookup->obj.klass||lookup->length>1024||a.class_get_rank(lookup->obj.klass)!=1||a.class_get_element_class(lookup->obj.klass)!=refs.lookupRowClass)return;
    const auto fx=coordinate(cell,refs.frameX),fy=coordinate(cell,refs.frameY);
    auto** rows=static_cast<Il2CppArray**>(arrayData(lookup));
    // The original loader falls back to style zero when the engine's current
    // coordinates are not present in the lookup table.
    Il2CppArray* selected=lookup->length&&lookupRow(rows[0])?rows[0]:nullptr;
    for(uintptr_t i=0;i<lookup->length;++i){auto* row=rows[i];if(!lookupRow(row)||n<0||static_cast<uintptr_t>(n)>=row->length)continue;
        if(point(row,static_cast<size_t>(n))==std::pair{fx,fy}){selected=row;break;}
    }
    if(!selected)return;
    const int replacement=d.choices[randomValue()%d.choices.size()];if(static_cast<uintptr_t>(replacement)>=selected->length)return;
    const auto [px,py]=point(selected,static_cast<size_t>(replacement));setNumber(cell,replacement);setCoordinate(cell,px,refs.setX);setCoordinate(cell,py,refs.setY);
}
void area(int32_t x0,int32_t x1,int32_t y0,int32_t y1,const MethodInfo* m) {
    const auto view=std::atomic_load(&table);runtime::TileArrays world{};
    if(!view->empty()&&runtime::tileArrays(&world)&&world.wall) {
        bool day=true;il2cpp::api().field_static_get_value(refs.day,&day);
        // Match the original loader's padded region. Native scans/lighting do
        // not enter Mono or QuickJS once per cell.
        const bool emits=std::any_of(view->begin(),view->end(),[day](const auto& pair){const auto& rgb=day?pair.second.definition.day:pair.second.definition.night;return rgb[0]>0||rgb[1]>0||rgb[2]>0;});
        if(!emits){if(originalArea)originalArea(x0,x1,y0,y1,m);return;}
        const int left=std::max<int64_t>(0,int64_t(x0)-28),right=std::min<int64_t>(world.width-1,int64_t(x1)+28);
        const int top=std::max<int64_t>(0,int64_t(y0)-28),bottom=std::min<int64_t>(world.height-1,int64_t(y1)+28);
        auto add=reinterpret_cast<void(*)(int32_t,int32_t,float,float,float,const MethodInfo*)>(il2cpp::methodPointer(refs.addLight));
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){auto found=view->find(world.wall[static_cast<size_t>(y)*world.width+x]);if(found==view->end())continue;
            const auto& rgb=day?found->second.definition.day:found->second.definition.night;
            if(rgb[0]>0||rgb[1]>0||rgb[2]>0)add(x,y,rgb[0],rgb[1],rgb[2],refs.addLight);
        }
    }
    if(originalArea)originalArea(x0,x1,y0,y1,m);
}
bool prepare(std::string* error) {
    auto& a=il2cpp::api();auto* main=il2cpp::findClass({"Terraria","Main",{}});auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    auto* framing=il2cpp::findClass({"Terraria","Framing",{}});auto* lighting=il2cpp::findClass({"Terraria","Lighting",{}});
    if(!a.class_get_rank||!a.class_get_element_class||!a.class_get_fields||!a.class_get_namespace||!a.class_get_name||
       !a.field_get_offset||!a.field_static_get_value||!a.runtime_invoke||!a.method_get_param) {
        *error="Stage=BindWall Expected=Array,Field,Value,MethodAndInvokeMetadata Actual=RequiredExportMissing Reason=MetadataUnavailable";return false;
    }
    Refs r;r.tile=il2cpp::findClass({"Terraria","Tile",{}});uint32_t alignment=0;
    if(!r.tile||!a.class_value_size||a.class_value_size(r.tile,&alignment)!=4||alignment!=4){*error="Type=Terraria.Tile Expected=Value4:Align4 Reason=NativeTileAbiMismatch";return false;}
    if(!il2cpp::nativeFieldContract(main,"wallHouse","System.Boolean[]",true,sizeof(void*),&r.house,nullptr,error)||
       !il2cpp::nativeFieldContract(main,"wallBlend","System.Int32[]",true,sizeof(void*),&r.blend,nullptr,error)||
       !il2cpp::nativeFieldContract(main,"dayTime","System.Boolean",true,1,&r.day,nullptr,error)||
       !frameArray(main,"wallFrame",&r.frames,error)||!frameArray(main,"wallFrameCounter",&r.counters,error))return false;
    r.number=method(r.tile,"byte wallFrameNumber()",true,error);r.frameX=method(r.tile,"int wallFrameX()",true,error);r.frameY=method(r.tile,"int wallFrameY()",true,error);
    r.setNumber=method(r.tile,"void wallFrameNumber(byte wallFrameNumber)",true,error);r.setX=method(r.tile,"void wallFrameX(int wallFrameX)",true,error);r.setY=method(r.tile,"void wallFrameY(int wallFrameY)",true,error);
    r.newDust=method(il2cpp::findClass({"Terraria","Dust",{}}),"int NewDust(Vector2 Position, int Width, int Height, int Type, float SpeedX, float SpeedY, int Alpha, Color newColor, float Scale)",false,error);
    r.addLight=method(lighting,"void AddLight(int i, int j, float r, float g, float b)",false,error);
    if(!r.number||!r.frameX||!r.frameY||!r.setNumber||!r.setX||!r.setY||!r.newDust||!r.addLight)return false;
    r.lookup=framing?il2cpp::findField(framing,"wallFrameLookup"):nullptr;
    auto* outer=r.lookup?a.class_from_il2cpp_type(a.field_get_type(r.lookup)):nullptr;
    auto* inner=outer&&a.class_get_rank(outer)==1?a.class_get_element_class(outer):nullptr;
    r.lookupRowClass=inner;
    r.point=inner&&a.class_get_rank(inner)==1?a.class_get_element_class(inner):nullptr;
    il2cpp::ValueLayout layout;
    if(!r.lookup||!(a.field_get_flags(r.lookup)&0x10)||!r.point) {
        *error="Field=Framing.wallFrameLookup Expected=Static:Point[][] Actual=MissingOrIncompatible Reason=NativeTypeOrStorageMismatch";return false;
    }
    if(!il2cpp::resolveValueLayout(r.point,&layout,error))return false;
    if(layout.size!=4&&layout.size!=8){*error="Field=wallFrameLookup.Point ExpectedBytes=4Or8 Actual="+std::to_string(layout.size)+" Reason=NativeFrameLayoutMismatch";return false;}
    r.pointBytes=static_cast<int>(layout.size);r.coordinateBytes=r.pointBytes/2;
    for(int i=0;i<2;++i){auto* f=il2cpp::findField(r.point,i?"Y":"X");auto* t=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;size_t offset=0;uint32_t align=0;
        const char* expected=r.coordinateBytes==2?"Int16":"Int32";
        if(!f||(a.field_get_flags(f)&0x10)||!t||std::strcmp(a.class_get_namespace(t),"System")||std::strcmp(a.class_get_name(t),expected)||!a.class_is_valuetype(t)||a.class_value_size(t,&align)!=r.coordinateBytes||!layout.offset(a.field_get_offset(f),r.coordinateBytes,&offset)||offset!=size_t(i*r.coordinateBytes)) {
            *error="Field=wallFrameLookup.Point Expected=SequentialInt16OrInt32XY Reason=NativeFrameLayoutMismatch";return false;
        }
    }
    auto* vector=a.class_from_il2cpp_type(a.method_get_param(r.newDust,0));
    auto* color=a.class_from_il2cpp_type(a.method_get_param(r.newDust,7));
    uint32_t vectorAlign=0,colorAlign=0;
    if(!vector||!color||!a.class_is_valuetype(vector)||!a.class_is_valuetype(color)||
       a.class_value_size(vector,&vectorAlign)!=8||vectorAlign!=4||a.class_value_size(color,&colorAlign)!=4) {
        *error="API=Dust.NewDust Expected=Vector2:8BytesAlign4,Color:4Bytes Reason=NativeValueAbiMismatch";return false;
    }
    il2cpp::ValueLayout vectorLayout;
    if(!il2cpp::resolveValueLayout(vector,&vectorLayout,error))return false;
    for(int i=0;i<2;++i) {
        auto* f=il2cpp::findField(vector,i?"Y":"X");auto* t=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;size_t offset=0;uint32_t align=0;
        if(!f||(a.field_get_flags(f)&0x10)||!t||std::strcmp(a.class_get_namespace(t),"System")||std::strcmp(a.class_get_name(t),"Single")||
           !a.class_is_valuetype(t)||a.class_value_size(t,&align)!=4||!vectorLayout.offset(a.field_get_offset(f),4,&offset)||offset!=size_t(i*4)) {
            *error="API=Dust.NewDust Field=Vector2.XY Expected=SequentialSingleXY Reason=NativeValueAbiMismatch";return false;
        }
    }
    refs=r;
    auto* killM=method(world,"void KillWall(int i, int j, bool fail)",false,error);
    auto* dustM=method(world,"void KillWall_MakeWallDust(int i, int j, Tile tileCache)",false,error);
    auto* dropM=method(world,"int KillWall_GetItemDrops(Tile tileCache)",false,error);
    auto* animM=method(main,"void DoUpdate_AnimateWalls()",false,error);
    auto* frameM=method(framing,"void WallFrame(int i, int j, bool resetFrame)",false,error);
    auto* lightM=method(lighting,"void LightTiles(int firstX, int lastX, int firstY, int lastY)",false,error);
    if(!killM||!dustM||!dropM||!animM||!frameM||!lightM)return false;
    if(!tileMethod(dustM,2)||!tileMethod(dropM,0)){*error="API=WorldGen.KillWall Expected=Tile:Value4Align4 Reason=NativeTileParameterAbiMismatch";return false;}
    if((!originalKill&&!hook::install(killM,kill,&originalKill))||(!originalDust&&!hook::install(dustM,dust,&originalDust))||
       (!originalDrop&&!hook::install(dropM,drop,&originalDrop))||(!originalAnimate&&!hook::install(animM,animate,&originalAnimate))||
       (!originalFrame&&!hook::install(frameM,frame,&originalFrame))||(!originalArea&&!hook::install(lightM,area,&originalArea))){*error="Stage=InstallHooks Reason=NativeHookInstallationFailed";return false;}
    return true;
}
void installed(int first,int last) {
    std::lock_guard lock(mutex);auto next=std::make_shared<Table>(*std::atomic_load(&table));bool pending=false;
    // Engine table recreation must restore defaults, not just pending first loads.
    // Replacing map entries is idempotent and does not append duplicates.
    for(const auto& [type,p]:*next)if(type>=first&&type<=last&&!queued.contains(type))queued.emplace(type,p);
    for(auto it=queued.begin();it!=queued.end();){const int type=it->first;auto& p=it->second;
        if(type<first||type>last){pending=true;++it;continue;}
        if(!runtime::modWallAssetReady(type)) {
            diagnostic(p.definition,"TextureLoad","Expected=LoadedNativeTexture Actual=Missing Result=Disabled Reason=NativeWallAssetUnavailable");
            failed.insert(type);it=queued.erase(it);continue;
        }
        auto* house=array(refs.house);auto* blend=array(refs.blend);
        if(!il2cpp::nativeArrayContract(house,"Boolean",1,65536)||!il2cpp::nativeArrayContract(blend,"Int32",4,65536)||uintptr_t(type)>=house->length||uintptr_t(type)>=blend->length||
           !validArray(refs.frames,array(refs.frames.field),type)||!validArray(refs.counters,array(refs.counters.field),type)){
            diagnostic(p.definition,"ApplyDefaults","Reason=NativeTableBoundsOrTypeMismatch");failed.insert(type);it=queued.erase(it);continue;
        }
        std::vector<runtime::WallMapEntry> maps;
        for(const auto& map:p.definition.maps)maps.push_back({map.r,map.g,map.b,map.name});
        if(!runtime::setModWallMapEntries(type,maps)){diagnostic(p.definition,"Map","Reason=MapEntriesRejectedBeforeDefaultsCommit");failed.insert(type);it=queued.erase(it);continue;}
        static_cast<uint8_t*>(arrayData(house))[type]=p.definition.housing?1:0;
        if(p.blend>=0)static_cast<int32_t*>(arrayData(blend))[type]=p.blend;
        failed.erase(type);(*next)[type]=p;BL_INFO("[CSharp][Wall] Mod=%s Class=%s ContentId=%d Stage=ApplyDefaults Result=Ready",p.definition.mod.c_str(),p.definition.name.c_str(),type);
        it=queued.erase(it);
    }
    std::atomic_store(&table,std::shared_ptr<const Table>(next));
    if(pending)runtime::requestNativeWallDefaults();
}
}
LoadStatus loadStatus() {
    std::lock_guard lock(mutex);
    if(runtime::modWallsSettled())for(auto it=queued.begin();it!=queued.end();) {
        if(it->first>=runtime::wallTypeCount()) {
            diagnostic(it->second.definition,"InstallTables","Expected=InstalledWallType ActualCount="+std::to_string(runtime::wallTypeCount())+" Reason=NativeWallServiceSettledWithoutContent");
            failed.insert(it->first);it=queued.erase(it);
        } else ++it;
    }
    return {static_cast<int>(queued.size()),static_cast<int>(failed.size())};
}
bool publish(std::vector<Definition> definitions,std::string* error) {
    if(definitions.empty())return true;
    std::vector<Policy> staged;std::set<int> seen;
    for(auto& d:definitions){if(d.type<runtime::kVanillaWallCount||runtime::modWallTypeByName(d.mod,d.name)!=d.type||!seen.insert(d.type).second){*error="Reason=WallOwnershipMismatchOrDuplicate";return false;}
        Policy p;p.definition=std::move(d);
        if(!p.definition.blend.empty()&&!il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID","WallID",{}}),p.definition.blend.c_str(),&p.blend,error))return false;
        if(!p.definition.dust.empty()&&!il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID","DustID",{}}),p.definition.dust.c_str(),&p.dust,error))return false;
        for(auto& [frame,name]:p.definition.frameDust){int id=-1;if(!il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID","DustID",{}}),name.c_str(),&id,error))return false;p.frameDust[frame]=id;}
        if(p.blend>=runtime::kVanillaWallCount||p.dust>6000){*error="Reason=NativeWallOrDustIdOutOfRange";return false;}
        staged.push_back(std::move(p));
    }
    if(!prepare(error)){const auto& d=staged.front().definition;*error="[CSharp][ERROR][Wall] Mod="+d.mod+" Class="+d.name+" File="+d.source+" Stage=BindNative Result=Rejected Detail="+*error;return false;}
    {std::lock_guard lock(mutex);for(auto& p:staged)queued[p.definition.type]=std::move(p);}
    runtime::setNativeWallsInstalledHook(installed);runtime::requestNativeWallDefaults();return true;
}
}
