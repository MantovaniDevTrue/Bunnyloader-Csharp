#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "content/items/ModItems.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include "experiments/csharp/ManagedAssemblyBindings.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include "hook/HookManager.h"
#include "core/Log.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace bl::csharp {
using runtime::NativeRecipeRequest;
using runtime::modItemTypeByKey;
using runtime::itemTypeCount;
using runtime::kVanillaItemCount; namespace {
struct Policy {NativeRecipeRequest request;int index=0;};
std::map<uintptr_t,Policy> policies;
std::map<int,uintptr_t> byIndex;
auto& assemblyBindings = nativeRecipeAssemblyBindings();
bool currentManagedAssembly(const std::string& mod,const std::string& captured) {
    return assemblyBindings.isCurrent(mod,captured);
}
struct Slot {FieldInfo* field=nullptr;const MethodInfo* get=nullptr;const MethodInfo* set=nullptr;};
struct BoolValue {int offset=-1;const MethodInfo* getter=nullptr;};
struct Refs {
    bool ready=false,filter=false,consume=false,craft=false;
    FieldInfo *players=nullptr,*day=nullptr;
    Slot available,count,myPlayer;
    Il2CppClass *player=nullptr,*item=nullptr;
    int life=-1,lifeMax=-1,itemType=-1,maxStack=-1,adjTiles=-1,sink=-1;
    BoolValue desert,water,snow;
    const MethodInfo *source=nullptr,*grant=nullptr;
    Il2CppClass* entry=nullptr;bool entryValue=false;size_t entrySize=0;
    int entryStack=-1,entryType=-1;
    const MethodInfo *isGroup=nullptr,*group=nullptr,*placeholder=nullptr,*owned=nullptr;
} refs;
using FindFn=void(*)(bool,const MethodInfo*);
using CraftFn=void(*)(Il2CppObject*,Il2CppObject*,bool,const MethodInfo*);
using ConsumeRefFn=int(*)(Il2CppObject*,Il2CppObject*,void*,const MethodInfo*);
struct Entry8 {uint64_t a;};struct Entry16 {uint64_t a,b;};
using Consume8Fn=int(*)(Il2CppObject*,Il2CppObject*,Entry8,const MethodInfo*);
using Consume16Fn=int(*)(Il2CppObject*,Il2CppObject*,Entry16,const MethodInfo*);
FindFn originalFind=nullptr;CraftFn originalCraft=nullptr;
ConsumeRefFn originalConsumeRef=nullptr;Consume8Fn originalConsume8=nullptr;Consume16Fn originalConsume16=nullptr;
const MethodInfo* method(Il2CppClass* cls,const char* signature,bool instance,std::string* error) {
    auto* m=cls?il2cpp::findMethodBySignature(cls,il2cpp::parseSignature(signature)):nullptr;
    if(!m||il2cpp::api().method_is_instance(m)!=instance){*error="Method="+std::string(signature)+" Reason=NativeSignatureMismatch";return nullptr;}
    return m;
}
const MethodInfo* nativeGrantMethod(Il2CppClass* cls,const char* signature,std::string* error) {
    const auto expected=il2cpp::parseSignature(signature);
    bool ambiguous=false;
    const MethodInfo* renamed=nullptr;
    const MethodInfo* found=cls?il2cpp::findMethodBySignature(cls,expected,&ambiguous,&renamed):nullptr;
    if(!found&&!ambiguous)found=renamed;
    if(found&&il2cpp::api().method_is_instance(found))return found;
    *error="Method="+std::string(signature)+" Reason="+(ambiguous?"AmbiguousNativeMethod":"NativeSignatureMismatch");
    const auto actual=il2cpp::listOverloads(cls,expected.name);
    for(size_t i=0;i<actual.size()&&i<4;++i)*error+=" Candidate="+actual[i];
    return nullptr;
}
bool field(Il2CppClass* cls,const char* name,const char* type,bool stat,size_t bytes,FieldInfo** f,int* offset,std::string* error) {
    return il2cpp::nativeFieldContract(cls,name,type,stat,bytes,f,offset,error);
}
bool slot(Il2CppClass* cls,const char* name,const char* type,Slot* value,std::string* error,bool writable=true) {
    std::string ignored;
    if(field(cls,name,type,true,std::strcmp(type,"System.Int32")==0?4:sizeof(void*),&value->field,nullptr,&ignored))return true;
    std::string result=std::strcmp(type,"System.Int32")==0?"int":"int[]";
    value->get=method(cls,(result+" get_"+name+"()").c_str(),false,error);
    if(!value->get)return false;
    if(result=="int"&&writable)value->set=method(cls,("void set_"+std::string(name)+"(int value)").c_str(),false,error);
    return result!="int"||!writable||value->set;
}
int getInt(const Slot& value){int n=0;if(value.field)il2cpp::api().field_static_get_value(value.field,&n);
    else n=reinterpret_cast<int(*)(const MethodInfo*)>(il2cpp::methodPointer(value.get))(value.get);
    return n;}
void setInt(const Slot& value,int n){if(value.field)il2cpp::api().field_static_set_value(value.field,&n);
    else reinterpret_cast<void(*)(int,const MethodInfo*)>(il2cpp::methodPointer(value.set))(n,value.set);}
Il2CppArray* getArray(const Slot& value){Il2CppArray* array=nullptr;if(value.field)il2cpp::api().field_static_get_value(value.field,&array);
    else array=reinterpret_cast<Il2CppArray*(*)(const MethodInfo*)>(il2cpp::methodPointer(value.get))(value.get);
    return array;}
bool boolean(Il2CppClass* cls,const char* name,BoolValue* value,std::string* error) {
    std::string ignored;
    if(field(cls,name,"System.Boolean",false,1,nullptr,&value->offset,&ignored))return true;
    value->getter=method(cls,("bool get_"+std::string(name)+"()").c_str(),true,error);return value->getter;
}
bool getBool(Il2CppObject* obj,const BoolValue& value){if(value.offset>=0){uint8_t n=0;std::memcpy(&n,reinterpret_cast<char*>(obj)+value.offset,1);return n;}
    return reinterpret_cast<bool(*)(Il2CppObject*,const MethodInfo*)>(il2cpp::methodPointer(value.getter))(obj,value.getter);}
Il2CppObject* localPlayer(){Il2CppArray* players=nullptr;il2cpp::api().field_static_get_value(refs.players,&players);
    const int index=getInt(refs.myPlayer);auto& a=il2cpp::api();
    if(!players||!a.class_get_rank||!a.class_get_element_class||a.class_get_rank(players->obj.klass)!=1||
       a.class_get_element_class(players->obj.klass)!=refs.player||index<0||static_cast<uintptr_t>(index)>=players->length)return nullptr;
    return static_cast<Il2CppObject**>(arrayData(players))[index];}
int readInt(void* object,int offset){int n=0;std::memcpy(&n,static_cast<char*>(object)+offset,4);return n;}
std::string snapshot(Il2CppObject* player,int result,int type,int amount,bool quick,int available=0){
    uint8_t day=0;il2cpp::api().field_static_get_value(refs.day,&day);
    bool water=getBool(player,refs.water);Il2CppArray* tiles=nullptr;
    std::memcpy(&tiles,reinterpret_cast<char*>(player)+refs.adjTiles,sizeof tiles);
    if(il2cpp::nativeArrayContract(tiles,"Boolean",1)&&refs.sink>=0&&static_cast<uintptr_t>(refs.sink)<tiles->length)
        water=water||static_cast<const uint8_t*>(arrayData(tiles))[refs.sink];
    const int flags=(day?1:0)|(getBool(player,refs.desert)?2:0)|(water?4:0)|(getBool(player,refs.snow)?8:0)|(quick?16:0);
    return std::to_string(readInt(player,refs.life))+","+std::to_string(readInt(player,refs.lifeMax))+","+std::to_string(flags)+","+
        std::to_string(result)+","+std::to_string(type)+","+std::to_string(amount)+","+std::to_string(available)+",0,0";
}
void diagnose(const Policy& p,int kind,const std::string& reason) {
    struct Repeated {std::chrono::steady_clock::time_point next{};uint64_t suppressed=0;};
    static std::map<std::string,Repeated> repeated;
    const auto key=p.request.key+":"+std::to_string(kind);const auto now=std::chrono::steady_clock::now();
    auto& state=repeated[key];
    if(now<state.next){++state.suppressed;return;}
    const auto suppressed=state.suppressed;state.suppressed=0;state.next=now+std::chrono::seconds(5);
    const bool failure=reason.rfind("ERROR|",0)==0||reason.rfind("Invalid",0)==0||reason.rfind("GrantDependencyMissing",0)==0;
    bl::log::write(failure?ANDROID_LOG_ERROR:ANDROID_LOG_WARN,
        "[CSharp][%s][RecipeCallback] Mod=%s Class=%s File=%s Recipe=%s Stage=Invoke Kind=%d Result=Fallback SuppressedSinceLast=%llu Detail=%s",
        failure?"ERROR":"WARN",p.request.mod.c_str(),p.request.name.c_str(),p.request.source.c_str(),p.request.key.c_str(),kind,
        static_cast<unsigned long long>(suppressed),reason.c_str());
}
bool call(const Policy& policy,int kind,Il2CppObject* player,int type,int amount,bool quick,std::string* output,int budget=3000,int available=0) {
    std::string assembly=assemblyBindings.current(policy.request.mod);
    if(assembly.empty()||!player){diagnose(policy,kind,"AssemblyOrPlayerNotReady");return false;}
    if(!csharp::invokeManagedRecipe(policy.request.mod,assembly,policy.request.name,policy.index,kind,
        snapshot(player,policy.request.result,type,amount,quick,available),output,budget)) {diagnose(policy,kind,output->empty()?"Stage=Invoke Reason=EmptyTransportResponse":*output);return false;}
    if(!currentManagedAssembly(policy.request.mod,assembly))return false;
    if(output->rfind("ERROR|",0)==0||output->rfind("PENDING|",0)==0){diagnose(policy,kind,*output);return false;}return true;
}
void hkFind(bool delay,const MethodInfo* info){
    if(originalFind)originalFind(delay,info);
    if(!refs.ready||byIndex.empty())return;
    auto* player=localPlayer();auto* array=getArray(refs.available);const int count=getInt(refs.count);
    if(!player||!il2cpp::nativeArrayContract(array,"Int32",4)||count<0||static_cast<uintptr_t>(count)>array->length)return;
    auto* values=static_cast<int*>(arrayData(array));int kept=0;
    const auto until=std::chrono::steady_clock::now()+std::chrono::microseconds(3000);
    for(int i=0;i<count;++i){const int id=values[i];bool allow=true;auto it=byIndex.find(id);
        if(it!=byIndex.end()){const auto& policy=policies.at(it->second);
            if(policy.request.callbacks&1){std::string output;const int left=static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(until-std::chrono::steady_clock::now()).count());
                allow=left>0&&call(policy,0,player,0,0,false,&output,left)&&output=="1";}}
        if(allow)values[kept++]=id;
    }
    if(kept!=count)setInt(refs.count,kept);
}
int discount(Il2CppObject* recipe,Il2CppObject* player,void* entry,int original){
    auto it=policies.find(reinterpret_cast<uintptr_t>(recipe));if(it==policies.end()||!(it->second.request.callbacks&2)||!entry)return original;
    if(!refs.entryValue&&il2cpp::api().object_get_class(static_cast<Il2CppObject*>(entry))!=refs.entry)return original;
    const int stack=readInt(entry,refs.entryStack);if(stack<1||stack>9999)return original;
    int type=readInt(entry,refs.entryType);
    const bool grouped=reinterpret_cast<bool(*)(void*,const MethodInfo*)>(il2cpp::methodPointer(refs.isGroup))(entry,refs.isGroup);
    if(grouped){auto* group=reinterpret_cast<Il2CppObject*(*)(void*,const MethodInfo*)>(il2cpp::methodPointer(refs.group))(entry,refs.group);
        if(!group)return original;
        type=reinterpret_cast<int(*)(Il2CppObject*,const MethodInfo*)>(il2cpp::methodPointer(refs.placeholder))(group,refs.placeholder);}
    int owned=0;
    if(!refs.entryValue)owned=reinterpret_cast<int(*)(void*,const MethodInfo*)>(il2cpp::methodPointer(refs.owned))(entry,refs.owned);
    else if(refs.entrySize<=8){Entry8 value{};std::memcpy(&value,entry,8);owned=reinterpret_cast<int(*)(Entry8,const MethodInfo*)>(il2cpp::methodPointer(refs.owned))(value,refs.owned);}
    else {Entry16 value{};std::memcpy(&value,entry,16);owned=reinterpret_cast<int(*)(Entry16,const MethodInfo*)>(il2cpp::methodPointer(refs.owned))(value,refs.owned);}
    const int amount=static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(stack)-original,0,stack)),limit=std::max(stack,owned);std::string output;
    if(!call(it->second,1,player,type,amount,false,&output,3000,limit))return original;
    int changed=0;auto parsed=std::from_chars(output.data(),output.data()+output.size(),changed);
    if(parsed.ec!=std::errc{}||parsed.ptr!=output.data()+output.size())return original;
    return stack-std::clamp(changed,0,limit);
}
int hkConsumeRef(Il2CppObject* recipe,Il2CppObject* player,void* entry,const MethodInfo* method){
    return discount(recipe,player,entry,originalConsumeRef?originalConsumeRef(recipe,player,entry,method):0);}
int hkConsume8(Il2CppObject* recipe,Il2CppObject* player,Entry8 entry,const MethodInfo* method){
    return discount(recipe,player,&entry,originalConsume8?originalConsume8(recipe,player,entry,method):0);}
int hkConsume16(Il2CppObject* recipe,Il2CppObject* player,Entry16 entry,const MethodInfo* method){
    return discount(recipe,player,&entry,originalConsume16?originalConsume16(recipe,player,entry,method):0);}
void grant(const Policy& policy,Il2CppObject* player,const std::string& wire){
    struct Command{int type,count;};std::vector<Command> commands;size_t start=0;
    while(start<wire.size()){
        const auto end=wire.find(';',start),stop=end==std::string::npos?wire.size():end,colon=wire.find(':',start);
        if(commands.size()>=8||colon==std::string::npos||colon>=stop){diagnose(policy,2,"InvalidGrantCommands");return;}
        const auto key=wire.substr(start,colon-start);int count=0,type=-1;
        const auto parsed=std::from_chars(wire.data()+colon+1,wire.data()+stop,count);
        if(parsed.ec!=std::errc{}||parsed.ptr!=wire.data()+stop||count<1||count>9999||key.size()<2){diagnose(policy,2,"InvalidGrantAmount");return;}
        if(key[0]=='v'){const auto id=std::from_chars(key.data()+1,key.data()+key.size(),type);
            if(id.ec!=std::errc{}||id.ptr!=key.data()+key.size()||type<1||type>=kVanillaItemCount){diagnose(policy,2,"InvalidVanillaGrant");return;}}
        else if(key[0]=='m')type=modItemTypeByKey(key.find('/')==std::string::npos?policy.request.mod+"/"+key.substr(1):key.substr(1));
        if(type<1||type>=itemTypeCount()){diagnose(policy,2,"GrantDependencyMissing:"+key);return;}
        commands.push_back({type,count});start=stop+1;
    }
    auto& a=il2cpp::api();int misc=0;void* sourceArgs[]={&misc};Il2CppObject* exception=nullptr;
    auto* source=a.runtime_invoke(refs.source,player,sourceArgs,&exception);
    if(!source||exception){diagnose(policy,2,"GetItemSource_MiscException");return;}
    const auto root=a.gchandle_new(source,false);if(!root){diagnose(policy,2,"SourceRootAllocationFailed");return;}
    for(auto& command:commands){void* args[]={source,&command.type,&command.count};exception=nullptr;
        a.runtime_invoke(refs.grant,player,args,&exception);if(exception){diagnose(policy,2,"QuickSpawnItemException");break;}}
    a.gchandle_free(root);
}
void hkCraft(Il2CppObject* recipe,Il2CppObject* result,bool quick,const MethodInfo* method){
    if(originalCraft)originalCraft(recipe,result,quick,method);
    auto it=policies.find(reinterpret_cast<uintptr_t>(recipe));
    if(it!=policies.end()&&(it->second.request.callbacks&4)){std::string output;auto* player=localPlayer();
        if(call(it->second,2,player,0,0,quick,&output)&&output.rfind("OK|",0)==0&&!output.substr(3).empty())grant(it->second,player,output.substr(3));}
}
} // namespace
void bindNativeRecipeAssembly(const std::string& mod,const std::string& assembly){
    assemblyBindings.bind(mod,assembly);
}
void unbindNativeRecipeAssembly(const std::string& mod){
    const std::string old=assemblyBindings.current(mod);
    if(old.empty())return;
    if(managedRecipeRuntimeReady()) {
        std::string response;
        invokeManagedRecipe(mod,old,"__unload__",0,6,"",&response,1500);
    }
    assemblyBindings.unbindIfCurrent(mod,old);
}
bool hasManagedAnglerQuestCallbacks(const std::string& mod) {
    const auto quests=angler::registry().snapshot();
    return std::any_of(quests.begin(),quests.end(),
        [&](const angler::Quest& q) { return q.mod==mod&&q.callbackMask!=0; });
}
bool invokeManagedAnglerQuestCallback(const std::string& mod,const std::string& owner,
                                      int kind,int first,int second,int itemType,bool* allowed,int playerIndex) {
    if (!allowed || (kind!=4&&kind!=5) || first<0 || first>1 ||
        second<0 || second>1 || itemType<0 || itemType>=65536 ||
        (kind==5 && (playerIndex<0||playerIndex>255)) ||
        !managedRecipeRuntimeReady()) return false;
    std::string assembly=assemblyBindings.current(mod);
    if(assembly.empty())return false;
    const std::string input=kind==4?
        std::to_string(first)+","+std::to_string(itemType):
        std::to_string(first)+","+std::to_string(second)+","+
        std::to_string(playerIndex)+","+std::to_string(itemType);
    std::string output;
    if(!invokeManagedRecipe(mod,assembly,owner,0,kind,input,&output,1500) ||
       !currentManagedAssembly(mod,assembly) ||
       (output!="1"&&output!="0"))return false;
    *allowed=output=="1";
    return true;
}
bool prepareNativeRecipeCallbacks(int mask,std::string* error){
    if(!mask)return true;
    if(mask<0||mask>7){*error="Reason=InvalidRecipeCallbackMask";return false;}
    auto& a=il2cpp::api();
    if(!a.object_get_class||!a.method_is_instance||!a.runtime_invoke||!a.field_static_get_value||!a.field_static_set_value||
       !a.class_get_rank||!a.class_get_element_class){*error="Stage=BindCallbacks Reason=RequiredExportMissing";return false;}
    auto* main=il2cpp::findClass({"Terraria","Main",{}});auto* recipe=il2cpp::findClass({"Terraria","Recipe",{}});
    if(!refs.ready){
        refs.player=il2cpp::findClass({"Terraria","Player",{}});refs.item=il2cpp::findClass({"Terraria","Item",{}});
        if(!main||!recipe||!refs.player||!refs.item||!a.gchandle_new||!a.gchandle_free){*error="Reason=MissingRecipeCallbackClassOrExport";return false;}
        if(!field(main,"player","Terraria.Player[]",true,sizeof(void*),&refs.players,nullptr,error)||
           !field(main,"dayTime","System.Boolean",true,1,&refs.day,nullptr,error)||
           !slot(main,"myPlayer","System.Int32",&refs.myPlayer,error,false)||
           !field(refs.player,"statLife","System.Int32",false,4,nullptr,&refs.life,error)||
           !field(refs.player,"statLifeMax2","System.Int32",false,4,nullptr,&refs.lifeMax,error)||
           !boolean(refs.player,"ZoneDesert",&refs.desert,error)||!boolean(refs.player,"adjWaterSource",&refs.water,error)||
           !boolean(refs.player,"ZoneSnow",&refs.snow,error)||
           !field(refs.player,"adjTile","System.Boolean[]",false,sizeof(void*),nullptr,&refs.adjTiles,error)||
           !il2cpp::readNativeId(il2cpp::findClass({"Terraria.ID","TileID",{}}),"Sinks",&refs.sink,error))return false;
        if(refs.sink>=754){*error="Field=TileID.Sinks Reason=InvalidVanillaTileId";return false;}
        refs.ready=true;
    }
    if((mask&1)&&!refs.filter){
        if(!slot(main,"availableRecipe","System.Int32[]",&refs.available,error)||!slot(main,"numAvailableRecipes","System.Int32",&refs.count,error))return false;
        const auto* find=method(recipe,"void FindRecipes(bool canDelayCheck)",false,error);
        if(!find||!hook::install(find,hkFind,&originalFind)){*error+=" Hook=FindRecipes Reason=HookInstallFailed";return false;}refs.filter=true;
    }
    if((mask&2)&&!refs.consume){
        refs.entry=il2cpp::findClass({"Terraria","Recipe","RequiredItemEntry"});
        if(!refs.entry){*error="Class=Recipe.RequiredItemEntry Reason=MissingClass";return false;}
        refs.entryValue=a.class_is_valuetype(refs.entry);uint32_t alignment=0;
        refs.entrySize=refs.entryValue?static_cast<size_t>(a.class_value_size(refs.entry,&alignment)):a.class_instance_size(refs.entry);
        if(refs.entryValue){
            il2cpp::ValueLayout layout;
            if((refs.entrySize!=8&&refs.entrySize!=12&&refs.entrySize!=16)||alignment>8||!il2cpp::resolveValueLayout(refs.entry,&layout,error)){
                *error="Type=RequiredItemEntry Expected=ReferenceOrIntegerAggregate8Or12Or16 ActualBytes="+std::to_string(refs.entrySize)+" Reason=UnsupportedAbi";return false;}
            // Integer aggregates use general-purpose registers. Reject floating/HFA layouts.
            void* cursor=nullptr;
            while(auto* f=a.class_get_fields(refs.entry,&cursor)) {
                if(a.field_get_flags(f)&0x10u)continue;
                auto* cls=a.class_from_il2cpp_type(a.field_get_type(f));
                if(!cls){*error="Type=RequiredItemEntry Reason=MissingFieldType";return false;}
                if(a.class_is_valuetype(cls)) {
                    const char* ns=a.class_get_namespace(cls),*name=a.class_get_name(cls);
                    if(!ns||!name||std::strcmp(ns,"System")||
                       (std::strcmp(name,"Int32")&&std::strcmp(name,"UInt32")&&std::strcmp(name,"Int16")&&std::strcmp(name,"UInt16")&&
                        std::strcmp(name,"Byte")&&std::strcmp(name,"SByte")&&std::strcmp(name,"Boolean")&&std::strcmp(name,"Int64")&&std::strcmp(name,"UInt64")&&
                        std::strcmp(name,"IntPtr")&&std::strcmp(name,"UIntPtr"))) {
                        *error="Type=RequiredItemEntry Expected=IntegerAggregate ActualFieldType="+std::string(name?name:"Missing")+" Reason=UnsupportedAggregateAbi";return false;
                    }
                }
            }
            for(const auto& member: {std::pair{"stack",&refs.entryStack},std::pair{"itemIdOrRecipeGroup",&refs.entryType}}){
                auto* f=il2cpp::findField(refs.entry,member.first);auto* cls=f?a.class_from_il2cpp_type(a.field_get_type(f)):nullptr;size_t offset=0;
                if(!f||(a.field_get_flags(f)&0x10u)||!cls||std::strcmp(a.class_get_name(cls),"Int32")||
                   std::strcmp(a.class_get_namespace(cls),"System")||a.class_value_size(cls,&alignment)!=4||!layout.offset(a.field_get_offset(f),4,&offset)){
                    *error="Field="+std::string(member.first)+" Expected=BoundedInt32 Reason=RequiredEntryLayoutMismatch";return false;}
                *member.second=static_cast<int>(offset);
            }
        }else if(!field(refs.entry,"stack","System.Int32",false,4,nullptr,&refs.entryStack,error)||
                 !field(refs.entry,"itemIdOrRecipeGroup","System.Int32",false,4,nullptr,&refs.entryType,error))return false;
        refs.isGroup=method(refs.entry,"bool get_IsRecipeGroup()",true,error);
        refs.group=method(refs.entry,"RecipeGroup get_RecipeGroup()",true,error);
        refs.placeholder=method(il2cpp::findClass({"Terraria","RecipeGroup",{}}),"int GetPlaceholderItemType()",true,error);
        const auto* consume=method(recipe,"int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)",true,error);
        refs.owned=method(recipe,"int GetAvailableItemCount(Recipe.RequiredItemEntry item)",false,error);
        if(!refs.isGroup||!refs.group||!refs.placeholder||!refs.owned||!consume)return false;
        bool installed=false;
        if(!refs.entryValue)installed=hook::install(consume,hkConsumeRef,&originalConsumeRef);
        else if(refs.entrySize<=8)installed=hook::install(consume,hkConsume8,&originalConsume8);
        else installed=hook::install(consume,hkConsume16,&originalConsume16);
        if(!installed){*error="Hook=GetIngredientCraftingDiscount Reason=HookInstallFailed";return false;}refs.consume=true;
    }
    if((mask&4)&&!refs.craft){
        refs.source=nativeGrantMethod(refs.player,"IEntitySource GetItemSource_Misc(int context)",error);
        if(!refs.source)return false;
        refs.grant=nativeGrantMethod(refs.player,"void QuickSpawnItem(IEntitySource source, int item, int stack)",error);
        if(!refs.grant)return false;
        const auto* craft=method(main,"void CraftItem_GrantItem(Recipe recipe, Item result, bool quickCraft)",false,error);
        if(!craft)return false;
        if(!hook::install(craft,hkCraft,&originalCraft)){*error="Hook=CraftItem_GrantItem Reason=HookInstallFailed";return false;}
        refs.craft=true;
    }
    return true;
}
runtime::RecipeCallbackReadiness prepareNativeRecipeCallbackProvider(const NativeRecipeRequest& request,std::string* error){
    using Ready=runtime::RecipeCallbackReadiness;
    if(!managedRecipeRuntimeReady()) {*error="Stage=WaitMonoWorker Reason=ManagedWorkerNotReady";return Ready::Pending;}
    if(!prepareNativeRecipeCallbacks(request.callbacks,error))return Ready::Rejected;
    std::string assembly=assemblyBindings.current(request.mod);
    const auto hash=request.key.rfind('#');int index=-1;
    if(hash!=std::string::npos){const auto token=request.key.substr(hash+1);const auto parsed=std::from_chars(token.data(),token.data()+token.size(),index);
        if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size())index=-1;}
    std::string output;
    if(assembly.empty()||index<0){*error="Stage=PrepareManagedHandlers Reason=AssemblyOrIdentityNotReady";return Ready::Pending;}
    if(!invokeManagedRecipe(request.mod,assembly,request.name,index,3,"",&output)) {
        *error=output.empty()?"Stage=PrepareManagedHandlers Reason=EmptyTransportResponse":output;
        return output.rfind("ERROR|",0)==0?Ready::Rejected:Ready::Pending;
    }
    if(output.rfind("PENDING|",0)==0){*error=output;return Ready::Pending;}
    if(output!="READY"){*error="Stage=PrepareManagedHandlers Expected=READY Actual="+output;return Ready::Rejected;}
    return Ready::Ready;
}
void trackNativeRecipeCallbacks(const NativeRecipeRequest& request,Il2CppObject* recipe,int index){
    if(!request.callbacks)return;
    const auto hash=request.key.rfind('#');int local=-1;
    if(hash!=std::string::npos){const auto text=request.key.substr(hash+1);auto n=std::from_chars(text.data(),text.data()+text.size(),local);
        if(n.ec!=std::errc{}||n.ptr!=text.data()+text.size())local=-1;}
    if(local<0||local>=16)return;
    const auto address=reinterpret_cast<uintptr_t>(recipe);policies[address]={request,local};byIndex[index]=address;
}
} // namespace bl::runtime
