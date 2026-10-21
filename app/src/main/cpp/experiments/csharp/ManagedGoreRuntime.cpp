#include "experiments/csharp/ManagedGoreRuntime.h"
#include "experiments/csharp/ManagedGoreCatalog.h"

#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "il2cpp/ValueLayout.h"
#include "boot/Boot.h"
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::gore {
namespace {

constexpr int kMaxTypeExclusive = 65536;
constexpr size_t kMaxQueue = 128;
constexpr size_t kMaxSpawnPerFrame = 32;
using Key = std::pair<std::string, std::string>;
using TypeIndex = std::map<Key, int>;

struct Vec2 { float x, y; };
struct Spawn {
    int type = 0;
    Vec2 position{}, velocity{};
    float scale = 1.f;
    uint64_t generation = 0;
};
struct GoreSettings {
    int special = 0;
    float speed = 1;
    float speedAlpha = 1;
    bool isDrip = false;
    bool safe = false;
};
struct ConfigRequest {
    int type = 0;
    GoreSettings values{};
};
struct NativeEntry {
    Entry entry;
    uint32_t root = 0;
    GoreSettings settings{};
    NativeEntry(const Entry& value, uint32_t handle) : entry(value), root(handle) {}
};
struct Tables {
    FieldInfo* texture = nullptr;
    FieldInfo* special = nullptr;
    FieldInfo* speed = nullptr;
    FieldInfo* speedAlpha = nullptr;
    FieldInfo* drip = nullptr;
    FieldInfo* safe = nullptr;
    const MethodInfo* setValue = nullptr;
    const MethodInfo* newGore = nullptr;
    size_t vectorX=0, vectorY=4;
    int speedKind=0, alphaKind=0; // 1 Byte, 2 Int32, 3 Single
    bool ready = false;
};

Tables g_tables;
std::mutex g_writer;
std::vector<NativeEntry> g_entries;
std::array<Il2CppArray*, 6> g_publishedArrays{};
std::array<uint32_t,6> g_tableRoots{};
std::shared_ptr<const TypeIndex> g_types = std::make_shared<const TypeIndex>();
std::mutex g_queueMutex;
std::deque<Spawn> g_queue;
std::deque<ConfigRequest> g_configs;
std::atomic<uint64_t> g_generation{1};
std::atomic<bool> g_worldReady{false};
uint64_t g_frame = 0;
int g_highWater = 0;
thread_local std::string lastRequestError;
thread_local unsigned requestFailures=0, suppressedRequests=0;
thread_local std::chrono::steady_clock::time_point lastRequestLog{};

int rejectRequest(const char* mod,const char* name,const char* stage,const char* reason,
                  const std::string& expected,const std::string& actual) {
    lastRequestError="[CSharp][WARN][GoreLoader] Mod="+std::string(mod ? mod : "Missing")+
        " Content="+(name ? name : "Missing")+" Stage="+stage+" Expected="+expected+
        " Actual="+actual+" Result=Rejected Reason="+reason;
    const auto now=std::chrono::steady_clock::now();
    if(++requestFailures<=3 || now-lastRequestLog>=std::chrono::seconds(2)) {
        BL_WARN("%s SuppressedSinceLast=%u",lastRequestError.c_str(),suppressedRequests);
        suppressedRequests=0;lastRequestLog=now;
    } else ++suppressedRequests;
    return 0;
}

bool failure(std::string* error,const std::string& stage,const std::string& field,
             const std::string& expected,const std::string& actual,const char* reason) {
    if(error) *error="[CSharp][ERROR][GoreLoader] Stage="+stage+" Field="+field+
        " Expected="+expected+" Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}

bool gameThread(std::string* error) {
    const int expected=runtime::gameThreadId();
    return expected>0 && expected==static_cast<int>(gettid()) ? true :
        failure(error,"ThreadCheck","Thread","ObservedUnity:"+std::to_string(expected),
                std::to_string(gettid()),"WrongThread");
}

Il2CppArray* staticArray(FieldInfo* field) {
    Il2CppArray* value = nullptr;
    if (field) il2cpp::api().field_static_get_value(field, &value);
    return value;
}

bool initializedStaticArray(FieldInfo* field, const char* label, const char* expected,std::string* error=nullptr) {
    auto& a = il2cpp::api();
    if (!field || !a.field_get_flags || !(a.field_get_flags(field) & 0x10u) ||
        (a.field_get_flags(field) & 0x40u))
        return failure(error,"BindTables",label,"StaticArray","MissingOrWrongStorage","NativeStorageMismatch");
    // Validate the declaration before reading: treating a scalar as an array
    // pointer can crash even before any element type check is reached.
    const auto* type=a.field_get_type(field);
    auto* declared=type ? a.class_from_il2cpp_type(type) : nullptr;
    if(!declared || a.class_get_rank(declared)!=1)
        return failure(error,"BindTables",label,"VectorArray","OtherTypeOrRank","NativeTypeMismatch");
    auto* element=a.class_get_element_class(declared);
    if (!element) return failure(error,"BindTables",label,"ArrayElement","Missing","NativeTypeMismatch");
    uint32_t alignment=0;
    const char* ns=a.class_get_namespace(element), *name=a.class_get_name(element);
    const bool value=a.class_is_valuetype(element);
    const int size=value ? a.class_value_size(element,&alignment) : -1;
    const bool valid=!expected ? element==runtime::content::textureAssetClass() && !value :
        ns && !std::strcmp(ns,"System") && name && value &&
        (!std::strcmp(expected,"Boolean") ? !std::strcmp(name,"Boolean") && size==1 :
         (!std::strcmp(name,"Byte") && size==1) ||
         ((!std::strcmp(name,"Int32") || !std::strcmp(name,"Single")) && size==4));
    if(!valid) return failure(error,"BindTables",label,expected ? expected : "Asset<Texture2D>",
        std::string(ns ? ns : "?")+"."+(name ? name : "?")+":"+std::to_string(size),"NativeElementTypeOrSizeMismatch");
    Il2CppArray* array = staticArray(field);
    if (!array || array->length == 0 || array->length > kMaxTypeExclusive || array->bounds)
        return failure(error,"BindTables",label,"InitializedVectorWithin65536",
            array ? std::to_string(array->length) : "Null","ArrayUnavailableOrOutOfBounds");
    Il2CppClass* cls = a.object_get_class(reinterpret_cast<Il2CppObject*>(array));
    return cls==declared ? true : failure(error,"BindTables",label,"DeclaredArrayType","OtherRuntimeArrayType","NativeArrayTypeMismatch");
}

bool primitive(Il2CppClass* cls,const char* expected,int size) {
    auto& a=il2cpp::api();uint32_t align=0;
    const char* ns=cls ? a.class_get_namespace(cls) : nullptr;
    const char* name=cls ? a.class_get_name(cls) : nullptr;
    return ns && name && !std::strcmp(ns,"System") && !std::strcmp(name,expected) &&
        a.class_is_valuetype(cls) && a.class_value_size(cls,&align)==size;
}

bool spawnSignature(Tables& next,std::string* error) {
    auto& a=il2cpp::api();
    auto param=[&](int i){const auto* t=a.method_get_param(next.newGore,i);return t ? a.class_from_il2cpp_type(t) : nullptr;};
    auto* vector=param(0);
    const char* ns=vector ? a.class_get_namespace(vector) : nullptr;
    const char* name=vector ? a.class_get_name(vector) : nullptr;
    const auto* returnType=a.method_get_return_type(next.newGore);
    auto* returned=returnType ? a.class_from_il2cpp_type(returnType) : nullptr;
    if(!vector || param(1)!=vector || !ns || !name || std::strcmp(ns,"Microsoft.Xna.Framework") ||
       std::strcmp(name,"Vector2") || !primitive(param(2),"Int32",4) || !primitive(param(3),"Single",4) ||
       !primitive(returned,"Int32",4))
        return failure(error,"BindSpawn","Gore.NewGore","Int32(Vector2,Vector2,Int32,Single)","NativeSignatureMismatch","NativeMethodMismatch");
    il2cpp::ValueLayout layout;
    if(!il2cpp::resolveValueLayout(vector,&layout,error)) return false;
    if(layout.size!=sizeof(Vec2)) return failure(error,"BindSpawn","Vector2","8Bytes",std::to_string(layout.size),"NativeValueSizeMismatch");
    size_t offsets[2]{};const char* fields[]={"X","Y"};
    for(int i=0;i<2;++i) {
        auto* f=il2cpp::findField(vector,fields[i]);
        const auto* t=f ? a.field_get_type(f) : nullptr;
        if(!f || (a.field_get_flags(f)&0x10u) || !t || !primitive(a.class_from_il2cpp_type(t),"Single",4) ||
           !layout.offset(a.field_get_offset(f),4,&offsets[i]))
            return failure(error,"BindSpawn",std::string("Vector2.")+fields[i],"InstanceSingleWithinValue","MissingOrIncompatible","NativeFieldContractMismatch");
    }
    if(offsets[0]==offsets[1]) return failure(error,"BindSpawn","Vector2.X/Y","DistinctOffsets","Overlapping","NativeFieldOverlap");
    next.vectorX=offsets[0];next.vectorY=offsets[1];return true;
}

bool init(std::string* error) {
    if (g_tables.ready) return true;
    auto& a = il2cpp::api();
    std::string missing;
#define REQUIRE_GORE_EXPORT(name) if(!a.name) { if(!missing.empty()) missing+=','; missing+=#name; }
    REQUIRE_GORE_EXPORT(field_get_flags) REQUIRE_GORE_EXPORT(field_get_type)
    REQUIRE_GORE_EXPORT(class_from_il2cpp_type) REQUIRE_GORE_EXPORT(object_get_class)
    REQUIRE_GORE_EXPORT(class_get_rank) REQUIRE_GORE_EXPORT(class_get_element_class)
    REQUIRE_GORE_EXPORT(class_get_name) REQUIRE_GORE_EXPORT(class_get_namespace)
    REQUIRE_GORE_EXPORT(class_is_valuetype) REQUIRE_GORE_EXPORT(class_value_size)
    REQUIRE_GORE_EXPORT(field_static_get_value) REQUIRE_GORE_EXPORT(field_static_set_value)
    REQUIRE_GORE_EXPORT(gchandle_new) REQUIRE_GORE_EXPORT(gchandle_get_target)
    REQUIRE_GORE_EXPORT(gchandle_free) REQUIRE_GORE_EXPORT(runtime_invoke)
    REQUIRE_GORE_EXPORT(method_is_instance) REQUIRE_GORE_EXPORT(method_get_param)
    REQUIRE_GORE_EXPORT(method_get_return_type)
#undef REQUIRE_GORE_EXPORT
    if(!missing.empty()) return failure(error,"BindExports","IL2CPP","TypedArrays,GC,Invocation",missing,"RequiredExportMissing");
    auto* assets = il2cpp::findClass({"Terraria.GameContent", "TextureAssets", {}});
    auto* goreId = il2cpp::findClass({"Terraria.ID", "GoreID", {}});
    auto* sets = goreId ? il2cpp::findNested(goreId, "Sets") : nullptr;
    auto* safety = il2cpp::findClass({"Terraria.GameContent", "ChildSafety", {}});
    auto* gore = il2cpp::findClass({"Terraria", "Gore", {}});
    auto* array = il2cpp::findClass({"System", "Array", {}});
    if (a.runtime_class_init) {
        for (auto* c : {assets, sets, safety})
            if (c) a.runtime_class_init(c);
    }
    Tables next;
    next.texture = assets ? il2cpp::findField(assets, "Gore") : nullptr;
    next.special = sets ? il2cpp::findField(sets, "SpecialAI") : nullptr;
    next.speed = sets ? il2cpp::findField(sets, "DisappearSpeed") : nullptr;
    next.speedAlpha = sets ? il2cpp::findField(sets, "DisappearSpeedAlpha") : nullptr;
    next.drip = sets ? il2cpp::findField(sets, "IsDrip") : nullptr;
    next.safe = safety ? il2cpp::findField(safety, "SafeGore") : nullptr;
    next.setValue = array ? il2cpp::findMethodBySignature(array,
        il2cpp::parseSignature("void SetValue(object value, int index)")) : nullptr;
    next.newGore = gore ? il2cpp::findMethodBySignature(gore,
        il2cpp::parseSignature("int NewGore(Vector2 Position, Vector2 Velocity, int Type, float Scale)")) : nullptr;
    if (!initializedStaticArray(next.texture, "TextureAssets.Gore",nullptr,error) ||
        !initializedStaticArray(next.special, "GoreID.Sets.SpecialAI","Numeric",error) ||
        !initializedStaticArray(next.speed, "GoreID.Sets.DisappearSpeed","Numeric",error) ||
        !initializedStaticArray(next.speedAlpha, "GoreID.Sets.DisappearSpeedAlpha","Numeric",error) ||
        !initializedStaticArray(next.drip, "GoreID.Sets.IsDrip","Boolean",error) ||
        !initializedStaticArray(next.safe, "ChildSafety.SafeGore","Boolean",error)) return false;
    if (
        !next.setValue || !next.newGore ||
        !a.method_is_instance(next.setValue) || a.method_is_instance(next.newGore)) {
        return failure(error,"BindMethods","Gore.NewGore|Array.SetValue",
            "StaticNewGoreAndInstanceSetValue","MissingOrStorageMismatch","NativeMethodMismatch");
    }
    if(!spawnSignature(next,error)) return false;
    const auto numericKind=[&](FieldInfo* field){
        auto* cls=a.class_get_element_class(a.class_from_il2cpp_type(a.field_get_type(field)));
        const char* name=a.class_get_name(cls);
        return !std::strcmp(name,"Byte") ? 1 : !std::strcmp(name,"Int32") ? 2 : 3;
    };
    next.speedKind=numericKind(next.speed);next.alphaKind=numericKind(next.speedAlpha);
    next.ready = true;
    g_tables = next;
    return true;
}

bool setObject(Il2CppArray* array, int index, Il2CppObject* object) {
    if (!array || index < 0 || static_cast<uintptr_t>(index) >= array->length)
        return false;
    void* args[2] = {object, &index};
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(g_tables.setValue, array, args, &exception);
    return exception == nullptr;
}

bool writeInt(Il2CppArray* arr, int index, double number) {
    if (!arr || index < 0 || static_cast<uintptr_t>(index) >= arr->length)
        return false;
    auto& api = il2cpp::api();
    auto* elem = api.class_get_element_class(api.object_get_class(
        reinterpret_cast<Il2CppObject*>(arr)));
    if (!elem) return false;
    const char* name = api.class_get_name(elem);
    if (!name) return false;
    if (!std::strcmp(name, "Byte") || !std::strcmp(name, "Boolean")) {
        static_cast<uint8_t*>(arrayData(arr))[index] = static_cast<uint8_t>(number);
        return true;
    }
    if (!std::strcmp(name, "Int32")) {
        static_cast<int32_t*>(arrayData(arr))[index] = static_cast<int32_t>(number);
        return true;
    }
    if (!std::strcmp(name, "Single")) {
        static_cast<float*>(arrayData(arr))[index] = static_cast<float>(number);
        return true;
    }
    return false;
}

std::array<FieldInfo*, 6> fields() {
    return {g_tables.texture, g_tables.special, g_tables.speed,
            g_tables.speedAlpha, g_tables.drip, g_tables.safe};
}

bool writeSettings(std::array<Il2CppArray*, 6>& arrays,
                   int type, const GoreSettings& settings) {
    return writeInt(arrays[1], type, settings.special) &&
           writeInt(arrays[2], type, settings.speed) &&
           writeInt(arrays[3], type, settings.speedAlpha) &&
           writeInt(arrays[4], type, settings.isDrip ? 1 : 0) &&
           writeInt(arrays[5], type, settings.safe ? 1 : 0);
}

struct PinnedArrays {
    std::array<uint32_t, 6> handles{};
    ~PinnedArrays() {
        auto& api = il2cpp::api();
        for (uint32_t handle : handles)
            if (handle) api.gchandle_free(handle);
    }
    bool pin(size_t index, Il2CppArray* array) {
        if (!array) return false;
        auto& api = il2cpp::api();
        handles[index] = api.gchandle_new(
            reinterpret_cast<Il2CppObject*>(array), false);
        return handles[index] != 0;
    }
};

void publishTables(const std::array<Il2CppArray*,6>& arrays,PinnedArrays& pins) {
    auto& a=il2cpp::api();
    const std::array<FieldInfo*,6> targets=fields();
    for(size_t i=0;i<targets.size();++i) a.field_static_set_value(targets[i],arrays[i]);
    for(size_t i=0;i<g_tableRoots.size();++i) {
        if(g_tableRoots[i]) a.gchandle_free(g_tableRoots[i]);
        g_tableRoots[i]=pins.handles[i];pins.handles[i]=0;
    }
    // Retaining the last published vectors also prevents a reclaimed vector's
    // address being reused and defeating the array-identity repair check.
    g_publishedArrays=arrays;
}

void destroyUncommitted(const std::vector<uint32_t>& roots) {
    auto& a = il2cpp::api();
    for (auto handle : roots) {
        if (!handle) continue;
        if (auto* asset = a.gchandle_get_target(handle))
            runtime::content::destroyTextureAsset(asset);
        a.gchandle_free(handle);
    }
}

bool applyDefaults(std::array<Il2CppArray*, 6>& arrays, int start,
                   const std::vector<Entry>& entries,
                   const std::vector<uint32_t>& roots) {
    auto& a = il2cpp::api();
    for (size_t i = 0; i < entries.size(); ++i) {
        const int type = start + static_cast<int>(i);
        Il2CppObject* asset = a.gchandle_get_target(roots[i]);
        if (!asset || !setObject(arrays[0], type, asset) ||
            !writeSettings(arrays, type, GoreSettings{}))
            return false;
    }
    return true;
}

bool repairOnGameThread(std::string* error=nullptr) {
    if (!g_tables.ready || g_entries.empty()) return true;
    const auto f = fields();
    std::array<Il2CppArray*, 6> previous{}, candidate{};
    PinnedArrays pins;
    bool changed = false;
    for (size_t i = 0; i < f.size(); ++i) {
        static constexpr const char* names[]={"TextureAssets.Gore","GoreID.Sets.SpecialAI","GoreID.Sets.DisappearSpeed",
            "GoreID.Sets.DisappearSpeedAlpha","GoreID.Sets.IsDrip","ChildSafety.SafeGore"};
        if(!initializedStaticArray(f[i],names[i],i==0 ? nullptr : i>=4 ? "Boolean" : "Numeric",error)) return false;
        previous[i] = staticArray(f[i]);
        if (!previous[i] || previous[i]->length > kMaxTypeExclusive)
            return failure(error,"Repair","GoreArrays","BoundedInitializedArrays","MissingOrTooLarge","ArrayUnavailableOrOutOfBounds");
        if (previous[i] != g_publishedArrays[i] ||
            previous[i]->length < static_cast<uintptr_t>(g_highWater))
            changed = true;
    }
    if (!changed) return true;
    // Copiar o lote inteiro evita expor parcialmente tabelas refeitas e
    // preserva registros de outros mods que possuam arrays maiores.
    for (size_t i = 0; i < f.size(); ++i) {
        const auto length = std::max(previous[i]->length,
            static_cast<uintptr_t>(g_highWater));
        candidate[i] = runtime::TypeTables::resizedCopy(previous[i], length);
        if (!candidate[i] || !pins.pin(i, candidate[i]))
            return failure(error,"Repair","GoreArray:"+std::to_string(i),"AllocatedRootedArray","AllocationOrRootFailed","AllocationFailed");
    }
    for (const auto& record : g_entries) {
        const int type = record.entry.type;
        if (!setObject(candidate[0], type,
                il2cpp::api().gchandle_get_target(record.root)) ||
            !writeSettings(candidate, type, record.settings))
            return failure(error,"Repair",record.entry.modId+"/"+record.entry.source.name,
                "RetainedAssetAndTypedSettings","AssetOrArrayWriteRejected","NativeEntryRestoreFailed");
    }
    publishTables(candidate,pins);
    BL_INFO("csharp GoreLoader: tabelas nativas refeitas; %zu gores preservadas", g_entries.size());
    return true;
}

bool callSpawn(const Spawn& request,std::string* error) {
    if (!g_tables.ready || !g_tables.newGore) return false;
    alignas(Vec2) std::array<unsigned char,sizeof(Vec2)> position{},velocity{};
    std::memcpy(position.data()+g_tables.vectorX,&request.position.x,4);
    std::memcpy(position.data()+g_tables.vectorY,&request.position.y,4);
    std::memcpy(velocity.data()+g_tables.vectorX,&request.velocity.x,4);
    std::memcpy(velocity.data()+g_tables.vectorY,&request.velocity.y,4);
    int type = request.type;
    float scale = request.scale;
    void* args[] = {position.data(), velocity.data(), &type, &scale};
    Il2CppObject* exception = nullptr;
    auto& a=il2cpp::api();
    auto* boxed=a.runtime_invoke(g_tables.newGore, nullptr, args, &exception);
    if(exception) {
        std::array<char,2048> text{};
        if(a.format_exception)a.format_exception(exception,text.data(),int(text.size()));
        // il2cpp_format_exception may fill the entire buffer without a NUL.
        text.back()=0;
        return failure(error,"Invoke","Gore.NewGore","NativeInt32Result",a.format_exception ? text.data() : "NativeException,FormatterUnavailable","NativeException");
    }
    if(!boxed || !primitive(a.object_get_class(boxed),"Int32",4))
        return failure(error,"Invoke","Gore.NewGore","BoxedInt32",boxed ? "OtherType" : "Null","NativeReturnTypeMismatch");
    int index=-1;std::memcpy(&index,reinterpret_cast<const char*>(boxed)+sizeof(Il2CppObject),4);
    return index>=0 ? true : failure(error,"Invoke","Gore.NewGore","NonNegativeNativeIndex",std::to_string(index),"NativeSpawnRejected");
}

bool validText(const char* input, size_t max) {
    if (!input) return false;
    const size_t n = strnlen(input, max + 1);
    return n > 0 && n <= max;
}

}

bool install(const std::string& modId, const std::vector<Source>& sources,
             std::string* error) {
    if (!error) return false;
    error->clear();
    if (sources.empty()) return true;
    if (!gameThread(error)) return false;
    std::lock_guard guard(g_writer);
    if (!init(error)) return false;
    Catalog batch;
    if (!batch.stage(modId, sources, error)) return false;
    const auto f = fields();
    std::array<Il2CppArray*, 6> previous{}, expanded{};
    PinnedArrays pins;
    int first = std::max(1, g_highWater);
    for (size_t i = 0; i < f.size(); ++i) {
        static constexpr const char* names[]={"TextureAssets.Gore","GoreID.Sets.SpecialAI","GoreID.Sets.DisappearSpeed",
            "GoreID.Sets.DisappearSpeedAlpha","GoreID.Sets.IsDrip","ChildSafety.SafeGore"};
        if(!initializedStaticArray(f[i],names[i],i==0 ? nullptr : i>=4 ? "Boolean" : "Numeric",error)) return false;
        previous[i] = staticArray(f[i]);
        if (!previous[i]) {
            *error = "campo nativo de gore sem array";
            return false;
        }
        first = std::max(first, static_cast<int>(previous[i]->length));
    }
    if (sources.size() > static_cast<size_t>(kMaxTypeExclusive - first))
        return failure(error,"ReserveTypes",modId,"CapacityBelow65536",std::to_string(first+sources.size()),"NativeTypeCapacityExceeded");
    if(!batch.allocate(first, kMaxTypeExclusive, error)) return false;
    const int end = batch.endType();
    auto current = std::atomic_load_explicit(&g_types, std::memory_order_acquire);
    size_t alreadyRegistered=0;
    for (const auto& entry : batch.entries())
        if (current->contains({entry.modId, entry.source.name})) {
            ++alreadyRegistered;
            auto previous=std::find_if(g_entries.begin(),g_entries.end(),[&](const NativeEntry& e){
                return e.entry.modId==entry.modId && e.entry.source.name==entry.source.name;
            });
            if(previous==g_entries.end() || previous->entry.source.absolutePath!=entry.source.absolutePath ||
               previous->entry.source.width!=entry.source.width || previous->entry.source.height!=entry.source.height ||
               previous->entry.source.byteCount!=entry.source.byteCount)
                return failure(error,"Register",entry.modId+"/"+entry.source.name,"UnchangedProcessOwnedAsset",
                    "ChangedSource","ProcessRestartRequired");
        }
    if(alreadyRegistered==batch.entries().size()) return repairOnGameThread(error);
    if(alreadyRegistered) return failure(error,"Register",modId,"AtomicInitialAssetSet","MixedExistingAndNew","ProcessRestartRequired");
    if(g_entries.size()>4096 || sources.size()>4096-g_entries.size())
        return failure(error,"Register","GoreCatalog","AtMost4096",std::to_string(g_entries.size()+sources.size()),"ContentLimitExceeded");
    for (size_t i = 0; i < f.size(); ++i) {
        expanded[i] = runtime::TypeTables::resizedCopy(previous[i],
            static_cast<uintptr_t>(end));
        if (!expanded[i] || !pins.pin(i, expanded[i])) {
            *error = "nao foi possivel aumentar/fixar array nativo de gore no GC";
            return false;
        }
    }
    std::vector<uint32_t> roots;
    roots.reserve(sources.size());
    auto& api = il2cpp::api();
    for (const auto& entry : batch.entries()) {
        std::vector<unsigned char> png;
        if (!readOwnedPng(entry.source, &png, error)) {
            destroyUncommitted(roots);
            return false;
        }
        int width = 0, height = 0;
        // Decodifica a copia validada, sem reabrir um caminho alteravel.
        Il2CppObject* asset = runtime::content::loadTextureAsset(
            entry.source.absolutePath, png.data(), png.size(),
            entry.modId + "/Gores/" + entry.source.name, &width, &height);
        if (!asset || width != static_cast<int>(entry.source.width) ||
            height != static_cast<int>(entry.source.height)) {
            if (asset) runtime::content::destroyTextureAsset(asset);
            destroyUncommitted(roots);
            *error = "PNG nao decodificado ou tamanho alterado: " +
                     entry.source.relativePath;
            return false;
        }
        const uint32_t root = api.gchandle_new(asset, false);
        if (!root) {
            runtime::content::destroyTextureAsset(asset);
            destroyUncommitted(roots);
            *error = "nao foi possivel manter raiz GC de gore";
            return false;
        }
        roots.push_back(root);
    }
    if (!applyDefaults(expanded, first, batch.entries(), roots)) {
        destroyUncommitted(roots);
        *error = "falha ao preencher tabelas tipadas de gore";
        return false;
    }
    // Prepare C++ ownership/index allocations too, before exposing any native
    // arrays. A failed index allocation must not leave half-registered types.
    // Keep the core's -fno-exceptions ABI: no C++ catch/unwind may cross an
    // IL2CPP frame. Runtime-invoke reports managed exceptions via its out slot.
    auto next=std::make_shared<TypeIndex>(*current);
    auto combined=g_entries;
    combined.reserve(combined.size()+batch.entries().size());
    for (size_t i = 0; i < batch.entries().size(); ++i) {
        const auto& entry = batch.entries()[i];
        combined.emplace_back(entry, roots[i]);
        next->emplace(Key{entry.modId, entry.source.name}, entry.type);
        next->try_emplace(Key{"", entry.source.name}, entry.type);
    }
    // All assets, array roots, values and C++ ownership are now ready.
    publishTables(expanded,pins);
    g_entries.swap(combined);
    g_highWater = end;
    g_publishedArrays = expanded;
    std::atomic_store_explicit(&g_types,
        std::shared_ptr<const TypeIndex>(next), std::memory_order_release);
    BL_INFO("csharp GoreLoader %s: %zu gores nativas registradas (%d..%d)",
            modId.c_str(), sources.size(), first, end - 1);
    return true;
}

int typeOf(const std::string& modId, const std::string& name) {
    const auto view = std::atomic_load_explicit(&g_types, std::memory_order_acquire);
    const auto it = view->find({modId, name});
    return it == view->end() ? 0 : it->second;
}

void worldBoundary() {
    g_worldReady.store(false, std::memory_order_release);
    g_generation.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard lock(g_queueMutex);
    g_queue.clear();
}

void worldReady() {
    g_worldReady.store(true, std::memory_order_release);
}

void pumpOnGameThread() {
    if (runtime::gameThreadId()<=0 || runtime::gameThreadId()!=static_cast<int>(gettid())) return;
    if (!g_tables.ready) return;
    // Nao retirar configuracoes enquanto os arrays estiverem invalidos:
    // uma tentativa de recuperacao malsucedida nao pode perder settings.
    bool pending = false;
    {
        std::lock_guard lock(g_queueMutex);
        pending = !g_configs.empty() || !g_queue.empty();
    }
    std::string nativeError;
    if ((pending || ++g_frame % 20 == 0) && !repairOnGameThread(&nativeError)) {
        rejectRequest(nullptr,nullptr,"Pump","NativeTablesRejected","ValidTypedGoreTables",nativeError);
        return;
    }
    std::array<ConfigRequest, 32> configs{};
    size_t configured = 0;
    {
        std::lock_guard lock(g_queueMutex);
        while (configured < configs.size() && !g_configs.empty()) {
            configs[configured++] = g_configs.front();
            g_configs.pop_front();
        }
    }
    if (configured) {
        const auto f = fields();
        std::array<Il2CppArray*, 6> arrays{};
        for (size_t i = 0; i < f.size(); ++i)
            arrays[i] = staticArray(f[i]);
        for (size_t i = 0; i < configured; ++i) {
            const auto& request = configs[i];
            auto found = std::find_if(g_entries.begin(), g_entries.end(),
                [&](const NativeEntry& e) { return e.entry.type == request.type; });
            if (found == g_entries.end() || !writeSettings(arrays, request.type, request.values)) {
                rejectRequest(found==g_entries.end() ? nullptr : found->entry.modId.c_str(),
                    found==g_entries.end() ? nullptr : found->entry.source.name.c_str(),"ApplyConfiguration",
                    "NativeEntryOrWriteRejected","OwnedEntryAndWritableTypedSlots","Type:"+std::to_string(request.type));
                continue;
            }
            found->settings = request.values;
        }
    }
    if (!g_worldReady.load(std::memory_order_acquire)) return;
    std::array<Spawn, kMaxSpawnPerFrame> requests{};
    size_t count = 0;
    {
        std::lock_guard lock(g_queueMutex);
        while (count < requests.size() && !g_queue.empty()) {
            requests[count++] = g_queue.front();
            g_queue.pop_front();
        }
    }
    const uint64_t current = g_generation.load(std::memory_order_acquire);
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].generation == current && !callSpawn(requests[i],&nativeError)) {
            auto found=std::find_if(g_entries.begin(),g_entries.end(),[&](const NativeEntry& e){return e.entry.type==requests[i].type;});
            rejectRequest(found==g_entries.end() ? nullptr : found->entry.modId.c_str(),
                found==g_entries.end() ? nullptr : found->entry.source.name.c_str(),"InvokeNewGore",
                "NativeSpawnRejected","NativeGoreIndex",nativeError);
        }
    }
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_type(const char* modId, const char* name) {
    if (!validText(modId, 128) || !validText(name, 180)) return 0;
    return typeOf(modId, name);
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_type_any(const char* name) {
    if (!validText(name, 180)) return 0;
    return typeOf("", name);
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_configure_v2(const char* modId, const char* name,
                                int special, float speed, float speedAlpha,
                                int isDrip, int safe) {
    if (!validText(modId, 128) || !validText(name, 180) ||
        special < 0 || special > 255 ||
        !std::isfinite(speed) || speed < 0 || speed > 2147483520.f ||
        !std::isfinite(speedAlpha) || speedAlpha < 0 || speedAlpha > 2147483520.f ||
        (isDrip != 0 && isDrip != 1) ||
        (safe != 0 && safe != 1))
        return rejectRequest(nullptr,nullptr,"Configure","InvalidArgument","ValidIdentityAndFiniteNativeValues","InvalidIdentityOrValue");
    const int type = typeOf(modId, name);
    if (type <= 0) return rejectRequest(modId,name,"ResolveType","ContentNotFound","RegisteredOwnedGore","Missing");
    const auto representable=[](float value,int kind){
        return kind==3 || (std::floor(value)==value && (kind!=1 || value<=255));
    };
    if(!representable(speed,g_tables.speedKind) || !representable(speedAlpha,g_tables.alphaKind))
        return rejectRequest(modId,name,"Configure","NativeRepresentationMismatch",
            "SpeedKind:"+std::to_string(g_tables.speedKind)+",AlphaKind:"+std::to_string(g_tables.alphaKind)+"(1=Byte,2=Int32,3=Single)",
            "Speed:"+std::to_string(speed)+",Alpha:"+std::to_string(speedAlpha));
    const GoreSettings settings{special, speed, speedAlpha,
                                isDrip != 0, safe != 0};
    std::lock_guard lock(g_queueMutex);
    for (auto& existing : g_configs) {
        if (existing.type == type) {
            existing.values = settings;
            lastRequestError.clear();
            return 1;
        }
    }
    if (g_configs.size() >= kMaxQueue) return rejectRequest(modId,name,"QueueConfiguration","QueueFull","Below128",std::to_string(g_configs.size()));
    g_configs.push_back({type, settings});
    lastRequestError.clear();
    return 1;
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_configure(const char* modId,const char* name,int special,int speed,int alpha,int drip,int safe) {
    if(speed<0 || speed>255 || alpha<0 || alpha>255)
        return rejectRequest(nullptr,nullptr,"ConfigureLegacy","ArgumentOutOfRange","ByteCompatibleIntegerRates",
            std::to_string(speed)+","+std::to_string(alpha));
    return bunny_csharp_gore_configure_v2(modId,name,special,static_cast<float>(speed),static_cast<float>(alpha),drip,safe);
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_spawn(const char* modId, const char* name, float x, float y,
                           float vx, float vy, float scale) {
    if (!validText(modId, 128) || !validText(name, 180)) return rejectRequest(nullptr,nullptr,"QueueSpawn","InvalidIdentity","NonEmptyBoundedIdentity","MissingOrTooLong");
    if (!g_worldReady.load(std::memory_order_acquire)) return rejectRequest(modId,name,"QueueSpawn","WorldNotReady","ActiveWorld","Inactive");
    if (
        !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(vx) || !std::isfinite(vy) ||
        !std::isfinite(scale) || scale < 0.1f || scale > 4.f ||
        std::abs(x) > 100000000.f || std::abs(y) > 100000000.f ||
        std::abs(vx) > 10000.f || std::abs(vy) > 10000.f)
        return rejectRequest(modId,name,"QueueSpawn","InvalidPositionVelocityOrScale","FiniteBoundedValues","Position:"+std::to_string(x)+","+std::to_string(y)+",Velocity:"+std::to_string(vx)+","+std::to_string(vy)+",Scale:"+std::to_string(scale));
    const int type = typeOf(modId, name);
    if (type <= 0) return rejectRequest(modId,name,"ResolveType","ContentNotFound","RegisteredOwnedGore","Missing");
    const uint64_t generation = g_generation.load(std::memory_order_acquire);
    std::lock_guard lock(g_queueMutex);
    if (g_queue.size() >= kMaxQueue) return rejectRequest(modId,name,"QueueSpawn","QueueFull","Below128",std::to_string(g_queue.size()));
    if (!g_worldReady.load(std::memory_order_acquire) ||
        g_generation.load(std::memory_order_acquire) != generation)
        return rejectRequest(modId,name,"QueueSpawn","WorldSessionChanged","SameActiveWorld","ChangedOrInactive");
    g_queue.push_back({type, {x, y}, {vx, vy}, scale, generation});
    lastRequestError.clear();
    return 1;
}

extern "C" __attribute__((visibility("default"))) const char* bunny_csharp_gore_last_error() {
    return lastRequestError.c_str();
}

}
