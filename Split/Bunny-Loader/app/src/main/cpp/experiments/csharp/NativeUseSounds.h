#pragma once
#include "il2cpp/FieldContract.h"
#include "il2cpp/Signature.h"
#include "content/sounds/AndroidAudio.h"
#include "content/sounds/NativeSoundRouter.h"
#include <atomic>
#include <charconv>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <tuple>

namespace bl::csharp::useSounds {
struct Entry {
    std::string mod, name, source, directory, path;
    int type = -1;
    float volume = 1, pitch = 0, variance = 0;
    int maxInstances = 1, limit = 1;
};
struct Marker {
    uint32_t handle = 0;
    int style = -1, sound = 0;
    Marker(int value,int asset) : style(value),sound(asset) {}
    ~Marker() {
        if (handle) il2cpp::api().gchandle_free(handle);
        if (style>=0) runtime::content::releaseSoundStyle(style);
    }
};
struct Binding { int offset = -1; std::shared_ptr<Marker> marker; Entry definition; std::string resolvedPath; };
inline bool sameDefinition(const Binding& binding,const Entry& e,const std::string& path) {
    const auto& old=binding.definition;
    return old.mod==e.mod&&old.name==e.name&&binding.resolvedPath==path&&
        old.volume==e.volume&&old.pitch==e.pitch&&old.variance==e.variance&&
        old.maxInstances==e.maxInstances&&old.limit==e.limit;
}
using Table = std::map<int,Binding>;
inline std::shared_ptr<const Table> table = std::make_shared<const Table>();
inline std::mutex writer;
using ResourceKey=std::tuple<std::string,float,float,float,int,int>;
inline std::map<ResourceKey,std::weak_ptr<Marker>> markers;
inline std::map<std::string,int> audioAssets;

inline bool reject(const Entry& e, std::string* error, const std::string& stage,
                   const std::string& reason, const std::string& expected, const std::string& actual) {
    *error = "[CSharp][ERROR][Sound] Mod="+e.mod+" Class="+e.name+" File="+e.source+
        " API=CustomUseSound ContentId="+std::to_string(e.type)+" Path="+e.path+" Stage="+stage+
        " Expected="+expected+" Actual="+actual+" Reason="+reason+" Result=BatchRejected";
    return false;
}
inline bool decode(const std::vector<std::string>& fields,const Entry& context,Entry* result,std::string* error) {
    Entry e=context;
    if(fields.size()!=8||fields[0]!="USE_SOUND"||fields[1]!=context.name)
        return reject(e,error,"Decode","InvalidSoundWire","8FieldsAndMatchingItemName",std::to_string(fields.size()));
    const auto real=[](const std::string& text,float* value) {
        if(text.empty()||text.size()>40) return false;
        char* end=nullptr;double number=std::strtod(text.c_str(),&end);
        if(end!=text.c_str()+text.size()||!std::isfinite(number)||std::fabs(number)>4) return false;
        *value=static_cast<float>(number);return true;
    };
    const auto integer=[](const std::string& text,int* value) {
        if(text.empty()||text.size()>11) return false;
        auto parsed=std::from_chars(text.data(),text.data()+text.size(),*value);
        return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size();
    };
    if(!real(fields[3],&e.volume)||!real(fields[4],&e.pitch)||!real(fields[5],&e.variance)||
       !integer(fields[6],&e.maxInstances)||!integer(fields[7],&e.limit)||
       fields[2].empty()||fields[2].size()>360||fields[2].size()%2)
        return reject(e,error,"Decode","InvalidSoundOptions","FiniteSoundOptionsAndAudioPathHex","InvalidWireValues");
    const auto nibble=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;};
    e.path.clear();
    for(size_t i=0;i<fields[2].size();i+=2) {
        int hi=nibble(fields[2][i]),lo=nibble(fields[2][i+1]);
        if(hi<0||lo<0||((hi<<4)|lo)<32||((hi<<4)|lo)==127)
            return reject(e,error,"Decode","InvalidAssetPathHex","UTF8PathWithoutControls","MalformedHexOrControl");
        e.path.push_back(static_cast<char>((hi<<4)|lo));
    }
    *result=std::move(e);return true;
}

inline bool asset(const Entry& e, std::string* path, std::string* error) {
    namespace fs = std::filesystem;
    for(unsigned char c:e.path) if(c<32||c==127)
        return reject(e,error,"ResolveAsset","InvalidAssetPath","PathWithoutControls","ControlCharacter");
    const fs::path relative(e.path);
    if (e.path.empty() || e.path.size()>180 || relative.is_absolute() ||
        e.path.find_first_of("\\\t\r\n:|")!=std::string::npos)
        return reject(e,error,"ResolveAsset","InvalidAssetPath","RelativeAudioPath",e.path);
    for (const auto& part : relative)
        if (part==".." || part=="." || part.empty())
            return reject(e,error,"ResolveAsset","InvalidAssetPath","PathInsideModContent",e.path);
    std::string extension=relative.extension().string();
    for (auto& c : extension) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    if(extension!=".wav"&&extension!=".ogg"&&extension!=".mp3")
        return reject(e,error,"ResolveAsset","UnsupportedAudioExtension","Wav/Ogg/Mp3",extension);
    std::error_code ec;
    const fs::path root=fs::canonical(fs::path(e.directory)/"content",ec);
    if(ec) return reject(e,error,"ResolveAsset","ModContentUnavailable","ExistingContentDirectory",ec.message());
    const fs::path resolved=fs::canonical(root/relative,ec);
    if(ec) return reject(e,error,"ResolveAsset",ec==std::errc::no_such_file_or_directory||ec==std::errc::not_a_directory?"AssetMissing":"AssetPathResolutionFailed","ExistingAudioFile",ec.message()+":"+(root/relative).string());
    if(!fs::is_regular_file(resolved,ec))
        return reject(e,error,"ResolveAsset",ec?"AssetAccessFailed":"AssetNotRegularFile","RegularAudioFile",ec?ec.message():resolved.string());
    auto r=root.begin(),p=resolved.begin();
    for(;r!=root.end();++r,++p) if(p==resolved.end()||*r!=*p)
        return reject(e,error,"ResolveAsset","AssetOutsideModContent","PathInsideModContent",resolved.string());
    const auto size=fs::file_size(resolved,ec);
    if(ec||size==0||size>16*1024*1024)
        return reject(e,error,"ResolveAsset","InvalidAssetSize","1..16777216Bytes",ec?ec.message():std::to_string(size));
    *path=resolved.string(); return true;
}

inline bool publish(const std::vector<Entry>& entries, std::string* error) {
    if(entries.empty()) return true;
    std::lock_guard lock(writer);
    for(auto it=markers.begin();it!=markers.end();) {
        if(it->second.expired())it=markers.erase(it);else ++it;
    }
    auto& a=il2cpp::api(); const auto& first=entries.front();
    if(!a.object_new||!a.gchandle_new||!a.gchandle_free||!a.gchandle_get_target||!a.runtime_invoke||
       !a.method_is_instance||!a.class_from_il2cpp_type||!a.field_get_type||!a.class_is_valuetype||
       !a.class_value_size||!a.class_is_enum||
       (!a.gc_wbarrier_set_field&&(!a.gc_is_incremental||a.gc_is_incremental())))
        return reject(first,error,"BindSound","RequiredExportMissing","IL2CPP allocation/roots/invoke/GC/layout exports","Unavailable");
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* legacy=il2cpp::findClass({"Terraria.Audio","LegacySoundStyle",{}});
    auto* soundType=il2cpp::findClass({"Terraria.Audio","SoundType",{}});
    FieldInfo* field=nullptr; int offset=-1;
    if(!legacy||a.class_is_valuetype(legacy))
        return reject(first,error,"BindSound","NativeSoundClassMismatch","LegacySoundStyleReferenceClass",legacy?"ValueType":"Missing");
    if(!il2cpp::nativeFieldContract(item,"UseSound","Terraria.Audio.LegacySoundStyle",false,sizeof(void*),&field,&offset,error))
        return reject(first,error,"BindSound","NativeSoundFieldMismatch","MutableInstanceLegacySoundStyle",*error);
    if(offset%alignof(void*)!=0)
        return reject(first,error,"BindSound","NativeFieldAlignmentMismatch","PointerAlignedUseSound",std::to_string(offset));
    if(a.field_get_flags(field)&0x20u)
        return reject(first,error,"BindSound","NativeStorageMismatch","MutableUseSound","InitOnly");
    uint32_t alignment=0;
    if(!soundType||!a.class_is_enum(soundType)||a.class_value_size(soundType,&alignment)!=4||alignment!=alignof(int))
        return reject(first,error,"BindSound","NativeEnumLayoutMismatch","SoundTypeEnum/4Bytes","MissingOrWrongSize");
    const auto* ctor=il2cpp::findMethodBySignature(legacy,il2cpp::parseSignature(
        "void .ctor(int soundId, int style, SoundType type, int maxTrackedInstances)"));
    if(!ctor||!a.method_is_instance(ctor))
        return reject(first,error,"BindSound","NativeConstructorMismatch","InstanceLegacySoundStyle(Int32,Int32,SoundType,Int32)","MissingOrStatic");
    // Validate the entire descriptor/asset batch before allocating/registering a style.
    std::vector<std::string> paths; std::map<int,bool> seen;
    for(const auto& e:entries) {
        if(e.type<0||!seen.emplace(e.type,true).second||!std::isfinite(e.volume)||e.volume<0||e.volume>4||
           !std::isfinite(e.pitch)||e.pitch < -1||e.pitch>1||!std::isfinite(e.variance)||e.variance<0||e.variance>2||
           e.maxInstances<0||e.maxInstances>256||(e.limit!=0&&e.limit!=1))
            return reject(e,error,"ValidateDescriptor","InvalidSoundDescriptor","UniqueItemAndFiniteSoundOptions","OutOfRangeOrDuplicate");
        std::string path; if(!asset(e,&path,error)) return false; paths.push_back(std::move(path));
    }
    const auto current=std::atomic_load_explicit(&table,std::memory_order_acquire);
    for(size_t i=0;i<entries.size();++i) {
        auto old=current->find(entries[i].type);
        if(old!=current->end()&&!sameDefinition(old->second,entries[i],paths[i]))
            return reject(entries[i],error,"Publish","ItemSoundAlreadyConfigured","SameSoundDefinitionOrGameRestart","DifferentSoundDefinition");
    }
    if(!runtime::content::installNativeSoundRouter())
        return reject(first,error,"BindRouter","NativeRouterRejected","InstalledNativeAudioRouter","NotInstalled");
    auto next=std::make_shared<Table>(*current);
    for(size_t i=0;i<entries.size();++i) {
        const auto& e=entries[i];
        if(current->contains(e.type)) continue; // Retry is idempotent; existing Item references retain their style.
        const ResourceKey key{paths[i],e.volume,e.pitch,e.variance,e.maxInstances,e.limit};
        auto cachedMarker=markers.find(key);
        if(cachedMarker!=markers.end())if(auto marker=cachedMarker->second.lock()) {
            const int decodeState=runtime::content::soundState(marker->sound);
            if(decodeState<0)return reject(e,error,"DecodeAsset","NativeAudioStateFailed","PendingOrReadyDecodeState",std::to_string(decodeState));
            (*next)[e.type]={offset,std::move(marker),e,paths[i]};continue;
        }
        auto loaded=audioAssets.find(paths[i]);
        const int sound=loaded==audioAssets.end()?runtime::content::loadSound(paths[i]):loaded->second;
        if(sound<=0)
            return reject(e,error,"LoadAsset","NativeAudioLoadRejected","PositiveSoundId",std::to_string(sound));
        if(loaded==audioAssets.end())audioAssets.emplace(paths[i],sound);
        const int decodeState=runtime::content::soundState(sound);
        if(decodeState<0)
            return reject(e,error,"DecodeAsset","NativeAudioStateFailed","PendingOrReadyDecodeState",std::to_string(decodeState));
        int style=runtime::content::registerSoundStyle(sound,e.volume,e.pitch,e.variance,e.maxInstances,e.limit);
        if(style<0) return reject(e,error,"RegisterStyle","NativeStyleRejected","NonNegativeStyleId",std::to_string(style));
        auto marker=std::make_shared<Marker>(style,sound);
        auto* object=a.object_new(legacy);
        if(!object) return reject(e,error,"CreateMarker","NativeAllocationFailed","LegacySoundStyleObject","Null");
        marker->handle=a.gchandle_new(object,false);
        if(!marker->handle) return reject(e,error,"RootMarker","NativeRootFailed","StrongGcHandle","Zero");
        if(a.gchandle_get_target(marker->handle)!=object)
            return reject(e,error,"RootMarker","NativeRootTargetMismatch","RootedAllocatedLegacySoundStyle","DifferentOrNullTarget");
        int markerId=runtime::content::kCustomSoundMarkerId,kind=0,maxTracked=0;
        void* args[]={&markerId,&style,&kind,&maxTracked};
        Il2CppObject* exception=nullptr; a.runtime_invoke(ctor,object,args,&exception);
        if(exception) {
            char detail[2048]{}; if(a.format_exception)a.format_exception(exception,detail,sizeof(detail));
            return reject(e,error,"CreateMarker","NativeConstructorException","SuccessfulLegacySoundStyleConstructor",detail[0]?detail:"NativeExceptionWithoutFormatter");
        }
        markers[key]=marker;
        (*next)[e.type]={offset,std::move(marker),e,paths[i]};
    }
    // No item is modified until every descriptor has a rooted marker.
    std::atomic_store_explicit(&table,std::shared_ptr<const Table>(std::move(next)),std::memory_order_release);
    return true;
}
inline void apply(Il2CppObject* item,int type) {
    if(!item) return;
    auto snapshot=std::atomic_load_explicit(&table,std::memory_order_acquire);
    auto found=snapshot->find(type); if(found==snapshot->end()) return;
    auto& a=il2cpp::api(); auto* sound=a.gchandle_get_target(found->second.marker->handle);
    auto** destination=reinterpret_cast<void**>(reinterpret_cast<char*>(item)+found->second.offset);
    if(a.gc_wbarrier_set_field) a.gc_wbarrier_set_field(item,destination,sound);
    else std::memcpy(destination,&sound,sizeof(sound)); // Only admitted for a confirmed nonincremental GC.
}
}
