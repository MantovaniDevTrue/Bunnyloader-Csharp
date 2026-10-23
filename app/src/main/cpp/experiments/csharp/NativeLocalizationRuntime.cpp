#include "experiments/csharp/NativeLocalizationRuntime.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeLocalizationSession.h"
#include "experiments/csharp/NativeLocalizationRetirePolicy.h"
#include "experiments/csharp/LocalizationResolve.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "hook/HookManager.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <set>
#include <tuple>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::localization {
namespace {
std::mutex gate;
NativeLocalizationSession session;
uint64_t lastAppliedGeneration = 0;
std::string lastCulture;
std::map<std::string, std::string> managedAssemblies;
std::map<std::string, std::string> lastManagedNotification;
std::set<std::string> pendingManagedNotification;
std::map<std::string, uint64_t> managedEpochs;
uint64_t nextManagedEpoch = 0;
uint64_t lastDynamicRevision = 0;
std::map<std::string, std::map<std::string, std::optional<std::string>>> vanillaOriginals;
std::set<std::string> originallyAbsentGlobalKeys;
uint32_t frameCounter = 0;
using LoadCultureFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
LoadCultureFn originalLoadCulture = nullptr;
bool loadHookInstalled = false;

bool identifier(const std::string& s, bool dot) {
    if (s.empty() || s.size() > 280 || s.front() == '.' || s.back() == '.') return false;
    for (char c : s) {
        const bool alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
        if (!alnum && c!='_' && c!='-' && (!dot || (c!='.' && c!='/' && c!='$'))) return false;
    }
    if(s.find("..")!=std::string::npos)return false;
    const size_t variant=s.find('$');
    if(variant!=std::string::npos &&
       (variant==0 || variant==s.size()-1 || s[variant-1]=='.' || s[variant-1]=='/' ||
        s.find('$',variant+1)!=std::string::npos ||
        s.find_first_of("./",variant+1)!=std::string::npos))return false;
    return true;
}

bool cultureId(const std::string& value) {
    if (value.size()<2 || value.size()>20) return false;
    for (char c:value) {
        if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
              (c>='0'&&c<='9')||c=='-')) return false;
    }
    return true;
}

bool validUtf8(const std::string& str) {
    for (size_t i=0;i<str.size();) {
        const auto c=static_cast<unsigned char>(str[i]);
        if(c<128){if(c==0 || (c<32 && c!='\n' && c!='\r' && c!='\t'))return false;++i;continue;}
        int length=c>=0xF0&&c<=0xF4?4:c>=0xE0&&c<=0xEF?3:c>=0xC2&&c<=0xDF?2:0;
        if(length==0||i+length>str.size())return false;
        for(int n=1;n<length;n++)if((static_cast<unsigned char>(str[i+n])&0xC0)!=0x80)return false;
        const auto next=static_cast<unsigned char>(str[i+1]);
        if((length==3&&c==0xE0&&next<0xA0)||(length==3&&c==0xED&&next>=0xA0)||
           (length==4&&c==0xF0&&next<0x90)||(length==4&&c==0xF4&&next>=0x90))return false;
        i+=length;
    }
    return true;
}

bool hex(const std::string& encoded, std::string* out, size_t max) {
    if(!out||encoded.size()%2!=0||encoded.size()>max*2)return false;
    out->clear();out->reserve(encoded.size()/2);
    auto nibble=[](char c)->int {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    };
    for(size_t i=0;i<encoded.size();i+=2) {
        int a=nibble(encoded[i]),b=nibble(encoded[i+1]);
        if(a<0||b<0)return false;
        out->push_back(static_cast<char>(a*16+b));
    }
    return validUtf8(*out);
}

std::string quote(const std::string& text) {
    static constexpr char digits[]="0123456789abcdef";
    std::string output="\"";
    for(unsigned char c:text) {
        if(c=='"'||c=='\\'){output+='\\';output+=static_cast<char>(c);}
        else if(c<32) {
            output+="\\u00";output+=digits[c>>4];output+=digits[c&15];
        } else output+=static_cast<char>(c);
    }
    return output+"\"";
}

std::string cultureName(Il2CppObject* culture, const MethodInfo* getter) {
    if (!culture || !getter) return {};
    auto& api=il2cpp::api();
    Il2CppObject* exception=nullptr;
    auto* text=reinterpret_cast<Il2CppString*>(api.runtime_invoke(getter,culture,nullptr,&exception));
    if(!text||exception||text->length<2||text->length>20)return {};
    std::string name;
    for(int i=0;i<text->length;i++) {
        const int ch=text->chars[i];
        if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
             (ch>='0'&&ch<='9')||ch=='-'))return {};
        name+=static_cast<char>(ch);
    }
    return name;
}

std::string textToUtf8(Il2CppString* text) {
    if (!text || text->length > 8192) return {};
    std::string out;
    out.reserve(static_cast<size_t>(text->length));
    for (int i=0;i<text->length;i++) {
        uint32_t c=static_cast<uint32_t>(text->chars[i]);
        if(c>=0xD800&&c<=0xDBFF) {
            if(++i>=text->length)return {};
            const uint32_t low=static_cast<uint32_t>(text->chars[i]);
            if(low<0xDC00||low>0xDFFF)return {};
            c=0x10000+((c-0xD800)<<10)+(low-0xDC00);
        } else if(c>=0xDC00&&c<=0xDFFF) return {};
        if(c<0x80)out.push_back(static_cast<char>(c));
        else if(c<0x800) {
            out.push_back(static_cast<char>(0xC0|(c>>6)));
            out.push_back(static_cast<char>(0x80|(c&63)));
        } else if(c<0x10000) {
            out.push_back(static_cast<char>(0xE0|(c>>12)));
            out.push_back(static_cast<char>(0x80|((c>>6)&63)));
            out.push_back(static_cast<char>(0x80|(c&63)));
        } else {
            out.push_back(static_cast<char>(0xF0|(c>>18)));
            out.push_back(static_cast<char>(0x80|((c>>12)&63)));
            out.push_back(static_cast<char>(0x80|((c>>6)&63)));
            out.push_back(static_cast<char>(0x80|(c&63)));
        }
    }
    return out;
}

struct Refs {
    FieldInfo* manager=nullptr;
    const MethodInfo* active=nullptr;
    const MethodInfo* name=nullptr;
    const MethodInfo* load=nullptr;
    const MethodInfo* exists=nullptr;
    const MethodInfo* getValue=nullptr;
    const MethodInfo* loadCulture=nullptr;
    const MethodInfo* invalidateTooltips=nullptr;
    bool valid=false;
};
Refs& refs() {
    static Refs reference;
    if (reference.valid) return reference;
    auto* lm=il2cpp::findClass({"Terraria.Localization","LanguageManager",{}});
    auto* language=il2cpp::findClass({"Terraria.Localization","Language",{}});
    auto* culture=il2cpp::findClass({"Terraria.Localization","GameCulture",{}});
    if(!lm||!language||!culture)return reference;
    reference.manager=il2cpp::findField(lm,"Instance");
    reference.active=il2cpp::findMethodBySignature(language,il2cpp::parseSignature("GameCulture get_ActiveCulture()"));
    reference.name=il2cpp::findMethodBySignature(culture,il2cpp::parseSignature("string get_Name()"));
    reference.load=il2cpp::findMethodBySignature(lm,il2cpp::parseSignature("void LoadLanguageFromFileText(string fileText)"));
    reference.exists=il2cpp::findMethodBySignature(language,il2cpp::parseSignature("bool Exists(string key)"));
    reference.getValue=il2cpp::findMethodBySignature(language,il2cpp::parseSignature("string GetTextValue(string key)"));
    reference.loadCulture=il2cpp::findMethodBySignature(lm,il2cpp::parseSignature("void LoadFilesForCulture(GameCulture culture)"));
    auto* tooltip=il2cpp::findClass({"Terraria.UI","ItemTooltip",{}});
    auto& api=il2cpp::api();
    const MethodInfo* invalidator=tooltip
        ? il2cpp::findMethodBySignature(tooltip,il2cpp::parseSignature("void InvalidateTooltips()"))
        : nullptr;
    reference.invalidateTooltips=invalidator && api.method_is_instance &&
        !api.method_is_instance(invalidator) ? invalidator : nullptr;
    reference.valid=reference.manager&&reference.active&&reference.name&&reference.load&&
        api.method_is_instance&&api.method_is_instance(reference.load)&&
        !api.method_is_instance(reference.active)&&api.method_is_instance(reference.name);
    return reference;
}

bool removeRetiredKeys(Il2CppObject* manager, const SessionSnapshot& current,
                       const std::set<std::string>& createdExternalKeys) {
    if (current.retired.empty()) return true;
    if (!safeToRetireAll(current, createdExternalKeys)) return false;
    static bool warned=false;
    const auto disable=[]() {
        if (!warned) {
            warned=true;
            BL_WARN("[CSharp][Localization] Stage=Unload Result=NeutralOnly Reason=MissingNativeRemove");
        }
        return true;
    };
    auto& api=il2cpp::api();
    auto* lm=il2cpp::findClass({"Terraria.Localization","LanguageManager",{}});
    if (!manager || !lm || !api.object_get_class || !api.method_is_instance ||
        !api.runtime_invoke || !api.string_new) return false;
    const int32_t offset=il2cpp::fieldOffset(lm,"_localizedTexts");
    if (offset < 0) return disable();
    auto* dictionary=*reinterpret_cast<Il2CppObject**>(
        reinterpret_cast<char*>(manager)+offset);
    if (!dictionary) return false;
    auto* dictionaryClass=api.object_get_class(dictionary);
    if (!dictionaryClass) return false;
    const auto* remove=il2cpp::findMethodBySignature(
        dictionaryClass,il2cpp::parseSignature("bool Remove(string key)"));
    if (!remove || !api.method_is_instance(remove)) return disable();
    for (const auto& key:current.retired) {
        void* args[]={api.string_new(key.c_str())};
        if (!args[0]) return false;
        Il2CppObject* exception=nullptr;
        api.runtime_invoke(remove,dictionary,args,&exception);
        if (exception) return disable();
    }
    return true;
}

bool publishToGame(const SessionSnapshot& current,const std::string& culture) {
    const auto& catalog=current.active;
    const auto& retired=current.retired;
    auto& api=il2cpp::api();auto& r=refs();
    if(!r.valid)return false;
    Il2CppObject* manager=nullptr;
    api.field_static_get_value(r.manager,&manager);
    if(!manager)return false;
    std::map<std::string,std::map<std::string,std::string>> selectedByMod;
    std::map<std::string,std::string> all;
    for(const auto& [modId,entries]:catalog) {
        auto& selected=selectedByMod[modId];
        for(const auto& entry:entries)selected.try_emplace(entry.key,entry.value);
        for(const auto& entry:entries)
            if(entry.culture=="en-US")selected[entry.key]=entry.value;
        for(const auto& entry:entries)
            if(entry.culture==culture)selected[entry.key]=entry.value;
        for(const auto& [key,value]:selected)all[key]=value;
    }
    bool lastLookupWasAbsent = false;
    const GameTextLookup vanilla=[&](const std::string& key,std::string* output)->bool {
        lastLookupWasAbsent = false;
        if(!r.exists||!r.getValue)return false;
        void* args[]={api.string_new(key.c_str())};
        if(!args[0])return false;
        Il2CppObject* exception=nullptr;
        Il2CppObject* boxed=api.runtime_invoke(r.exists,nullptr,args,&exception);
        if(exception||!boxed)return false;
        if(!(*reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(boxed)+sizeof(Il2CppObject)))) {
            lastLookupWasAbsent = true;
            return false;
        }
        exception=nullptr;
        auto* value=reinterpret_cast<Il2CppString*>(api.runtime_invoke(r.getValue,nullptr,args,&exception));
        if(exception||!value)return false;
        *output=textToUtf8(value);
        return !output->empty()||value->length==0;
    };
    auto& originals = vanillaOriginals[culture];
    for (const auto& [key, value] : all) {
        if (key.rfind("Mods.", 0) == 0 || originals.contains(key)) continue;
        std::string native;
        if (vanilla(key, &native))
            originals.emplace(key, std::move(native));
        else {
            if (!lastLookupWasAbsent ||
                (!originallyAbsentGlobalKeys.contains(key) &&
                 originallyAbsentGlobalKeys.size() >= 16384)) return false;
            originals.emplace(key, std::nullopt);
            originallyAbsentGlobalKeys.insert(key);
        }
    }
    std::map<std::string, std::string> retiredValues;
    SessionSnapshot ownedRetirement = current;
    ownedRetirement.retired.clear();
    std::set<std::string> createdExternalKeys;
    for (const auto& key : retired) {
        if (key.rfind("Mods.", 0) == 0) {
            ownedRetirement.retired.insert(key);
            retiredValues[key] = key;
        } else if (!all.contains(key)) {
            auto previous = originals.find(key);
            if (previous != originals.end() && previous->second.has_value()) {
                retiredValues[key] = *previous->second;
            } else if ((previous != originals.end() && !previous->second.has_value()) ||
                       originallyAbsentGlobalKeys.contains(key)) {
                retiredValues[key] = key;
                createdExternalKeys.insert(key);
                ownedRetirement.retired.insert(key);
            }
        }
    }
    for (const auto& [key,value] : retiredValues) all[key]=value;
    const auto resolved=resolveLocaleReferences(std::move(all),vanilla);
    const auto publishOne=[&](const std::string& owner,
                              const std::map<std::string,std::string>& entries)->bool {
        std::map<std::string,std::map<std::string,std::string>> categories;
        for(const auto& pair:entries) {
            const std::string& key=pair.first;
            auto match=resolved.find(key);
            if(match==resolved.end())return false;
            auto pos=key.rfind('.');
            if(pos==std::string::npos||pos==0||pos==key.size()-1)continue;
            categories[key.substr(0,pos)][key.substr(pos+1)]=match->second;
        }
        if(categories.empty())return true;
        std::string serialized="{";
        for(const auto& [category,items]:categories) {
            if(serialized.size()>1)serialized+=",";
            serialized+=quote(category)+":{";
            bool firstItem=true;
            for(const auto& [name,value]:items) {
                if(!firstItem)serialized+=",";
                firstItem=false;
                serialized+=quote(name)+":"+quote(value);
            }
            serialized+="}";
        }
        serialized+="}";
        if(serialized.size()>4*1024*1024)return false;
        void* args[]={api.string_new(serialized.c_str())};
        if(!args[0])return false;
        Il2CppObject* exception=nullptr;
        api.runtime_invoke(r.load,manager,args,&exception);
        if(exception) {
            BL_ERROR("[CSharp][ERROR][Localization] Mod=%s Stage=ApplyLanguage Reason=NativeException",owner.c_str());
            return false;
        }
        return true;
    };
    if(!retiredValues.empty() && !publishOne("retired",retiredValues)) return false;
    if(!ownedRetirement.retired.empty() &&
       !removeRetiredKeys(manager,ownedRetirement,createdExternalKeys))return false;
    for(const auto& [modId,entries]:selectedByMod)
        if(!publishOne(modId,entries))return false;
    return true;
}

void invalidateGameTooltips() {
    auto& r=refs();
    if(!r.invalidateTooltips)return;
    Il2CppObject* exception=nullptr;
    il2cpp::api().runtime_invoke(r.invalidateTooltips,nullptr,nullptr,&exception);
    if(exception)BL_WARN("[CSharp][Localization] Stage=Tooltips Reason=InvalidateFailed");
}

void notifyManagedApplied(const SessionSnapshot& snapshot, const std::string& culture, bool force) {
    const std::string token = std::to_string(snapshot.revision) + "|" + culture;
    std::lock_guard<std::mutex> lock(gate);
    if (snapshot.revision != session.revision() || lastCulture != culture) return;
    for (const auto& [modId, assembly] : managedAssemblies) {
        const auto generation = managedEpochs.find(modId);
        if (generation == managedEpochs.end() ||
            (!force && lastManagedNotification[modId] == token)) continue;
        if (bl::csharp::queueManagedLocalizationLoaded(modId, assembly, culture,
                                                       generation->second)) {
            lastManagedNotification[modId] = token;
            pendingManagedNotification.erase(modId);
        } else {
            lastManagedNotification.erase(modId);
            pendingManagedNotification.insert(modId);
            BL_WARN("[CSharp][Localization] Mod=%s Stage=CallbackQueue Reason=QueueFullOrStale",
                    modId.c_str());
        }
    }
}

void onLoadFilesForCulture(Il2CppObject* manager, Il2CppObject* culture, const MethodInfo* method) {
    if(originalLoadCulture)originalLoadCulture(manager,culture,method);
    auto& r=refs();
    const std::string name=cultureName(culture,r.name);
    if(name.empty())return;
    SessionSnapshot current;
    {
        std::lock_guard<std::mutex> lock(gate);
        current=session.snapshot();
    }
    if (current.active.empty() && current.retired.empty()) {
        std::lock_guard<std::mutex> lock(gate);
        if (managedAssemblies.empty()) return;
    }
    const uint64_t revision=current.revision;
    if(!publishToGame(current,name))return;
    invalidateGameTooltips();
    bool applied = false;
    {
        std::lock_guard<std::mutex> lock(gate);
        if(revision==session.revision()) {
            lastAppliedGeneration=revision;
            lastCulture=name;
            applied = true;
        }
    }
    if (applied) notifyManagedApplied(current, name, true);
}

bool installLanguageReloadHook() {
    if(loadHookInstalled)return true;
    auto& r=refs();
    if(!r.valid||!r.loadCulture)return false;
    if(!bl::hook::install(r.loadCulture,onLoadFilesForCulture,&originalLoadCulture))return false;
    loadHookInstalled=true;
    BL_INFO("[CSharp][Localization] Language reload hook active");
    return true;
}
}

bool decodeWire(const std::vector<std::string>& fields,const std::string& modId,Entry* result,std::string* error) {
    if(!result||!error)return false;
    if(fields.size()!=4||fields[0]!="LOCALE"||!identifier(modId,false)||
       !cultureId(fields[1])||
       !hex(fields[2],&result->key,280)||!hex(fields[3],&result->value,8192)||
       !identifier(result->key,true) ||
       (result->key.rfind("Mods.",0)==0 &&
        result->key.rfind("Mods.source.",0)!=0)) {
        *error="[CSharp][ERROR][Localization] Stage=Decode Result=Rejected Reason=InvalidLocaleWire";
        return false;
    }
    result->culture=fields[1];
    if (result->key.rfind("Mods.source.",0)==0)
        result->key="Mods."+modId+"."+result->key.substr(12);
    const std::string sourcePrefix="{$Mods.source.";
    size_t index=0;
    while((index=result->value.find(sourcePrefix,index))!=std::string::npos) {
        result->value.replace(index+7,6,modId);
        index+=7+modId.size();
    }
    return true;
}

bool decodeManagedWire(const std::string& wire, const std::string& modId,
                       std::vector<Entry>* result, std::string* error) {
    if (!result || !error || wire.size() > 262144 || !identifier(modId, false)) return false;
    auto reject = [&](const char* reason) {
        *error = reason;
        return false;
    };
    std::istringstream input(wire);
    std::string line;
    if (!std::getline(input, line) || line.rfind("OK|", 0) != 0)
        return reject("InvalidReplyHeader");
    int modules = -1;
    const char* start = line.data() + 3;
    auto [end, status] = std::from_chars(start, line.data() + line.size(), modules);
    if (status != std::errc() || end != line.data() + line.size() ||
        modules < 0 || modules > 32) return reject("InvalidModuleCount");
    if (!std::getline(input, line) || line != "BUNNY_DYNAMIC_LOCALES_V1")
        return reject("InvalidDynamicWireVersion");
    std::vector<Entry> entries;
    std::set<std::pair<std::string, std::string>> seen;
    const std::string prefix = "Mods." + modId + ".";
    while (std::getline(input, line)) {
        if (line.empty() || entries.size() >= 256) return reject("RowCountExceeded");
        const size_t a = line.find('\t');
        const size_t b = a == std::string::npos ? a : line.find('\t', a + 1);
        const size_t c = b == std::string::npos ? b : line.find('\t', b + 1);
        if (a == std::string::npos || b == std::string::npos ||
            c == std::string::npos || line.find('\t', c + 1) != std::string::npos ||
            line.substr(0, a) != "LOCALE") return reject("InvalidDynamicWireRow");
        Entry entry;
        entry.culture = line.substr(a + 1, b - a - 1);
        if (!cultureId(entry.culture) ||
            !hex(line.substr(b + 1, c - b - 1), &entry.key, 280) ||
            !hex(line.substr(c + 1), &entry.value, 8192) ||
            !identifier(entry.key, true) ||
            (entry.key.rfind("Mods.", 0) == 0 &&
             entry.key.rfind(prefix, 0) != 0) ||
            !seen.emplace(entry.key, entry.culture).second)
            return reject("InvalidDynamicWireIdentity");
        entries.push_back(std::move(entry));
    }
    *result = std::move(entries);
    return true;
}

bool acceptManagedWire(const std::string& modId, const std::string& assemblyPath,
                       const std::string& culture, uint64_t generation,
                       const std::string& wire) {
    std::vector<Entry> rows;
    std::string error;
    if (!decodeManagedWire(wire, modId, &rows, &error)) {
        BL_WARN("[CSharp][Localization] Mod=%s Stage=DynamicDecode Reason=%s",
                modId.c_str(), error.c_str());
        return false;
    }
    std::lock_guard<std::mutex> lock(gate);
    auto bound = managedAssemblies.find(modId);
    auto epoch = managedEpochs.find(modId);
    if (bound == managedAssemblies.end() || bound->second != assemblyPath ||
        epoch == managedEpochs.end() || epoch->second != generation ||
        culture != lastCulture) return false;
    if (session.replaceDynamic(modId, std::move(rows)))
        lastDynamicRevision = session.revision();
    return true;
}

void bindManagedAssembly(const std::string& modId, const std::string& assemblyPath) {
    if (modId.empty() || modId.size() > 64 ||
        assemblyPath.empty() || assemblyPath.size() > 2048) return;
    bl::csharp::setManagedLocalizationResultSink(&acceptManagedWire);
    std::lock_guard<std::mutex> lock(gate);
    auto previous = managedAssemblies.find(modId);
    if (previous != managedAssemblies.end() && previous->second == assemblyPath)
        return;
    if (previous != managedAssemblies.end())
        bl::csharp::unloadManagedLocalizationCallbacks(modId);
    managedAssemblies[modId] = assemblyPath;
    const uint64_t epoch = ++nextManagedEpoch;
    managedEpochs[modId] = epoch;
    lastManagedNotification.erase(modId);
    pendingManagedNotification.erase(modId);
    if (lastAppliedGeneration == session.revision() && !lastCulture.empty()) {
        const std::string token = std::to_string(session.revision()) + "|" + lastCulture;
        if (bl::csharp::queueManagedLocalizationLoaded(modId, assemblyPath, lastCulture, epoch))
            lastManagedNotification[modId] = token;
        else
            pendingManagedNotification.insert(modId);
    }
}

void retireMod(const std::string& modId) {
    std::lock_guard<std::mutex> lock(gate);
    session.replace(modId,{});
    // Always revoke the worker epoch, including a reload into the same DLL path.
    if(managedAssemblies.erase(modId)!=0)
        bl::csharp::unloadManagedLocalizationCallbacks(modId);
    managedEpochs.erase(modId);
    lastManagedNotification.erase(modId);
    pendingManagedNotification.erase(modId);
}

void stage(const std::string& modId,std::vector<Entry> entries) {
    std::lock_guard<std::mutex> lock(gate);
    session.replace(modId,std::move(entries));
}

void retainActiveMods(const std::set<std::string>& activeIds) {
    std::lock_guard<std::mutex> lock(gate);
    session.retainOnly(activeIds);
    for (auto it = managedAssemblies.begin(); it != managedAssemblies.end();) {
        if (activeIds.contains(it->first)) {
            ++it;
            continue;
        }
        bl::csharp::unloadManagedLocalizationCallbacks(it->first);
        lastManagedNotification.erase(it->first);
        pendingManagedNotification.erase(it->first);
        managedEpochs.erase(it->first);
        it = managedAssemblies.erase(it);
    }
}

void pumpOnGameThread() {
    if(++frameCounter%30!=0)return;
    installLanguageReloadHook();
    auto& api=il2cpp::api();auto& r=refs();
    if(!r.valid)return;
    Il2CppObject* exception=nullptr;
    Il2CppObject* active=api.runtime_invoke(r.active,nullptr,nullptr,&exception);
    if(exception)return;
    const std::string culture=cultureName(active,r.name);
    if(culture.empty())return;
    SessionSnapshot current;
    bool retryOnly = false;
    {
        std::lock_guard<std::mutex> lock(gate);
        retryOnly = session.revision() == lastAppliedGeneration && culture == lastCulture;
        if (retryOnly && pendingManagedNotification.empty()) return;
        current = session.snapshot();
    }
    if (retryOnly) {
        notifyManagedApplied(current, culture, false);
        return;
    }
    const uint64_t revision=current.revision;
    if(!publishToGame(current,culture))return;
    invalidateGameTooltips();
    bool applied = false;
    bool dynamicOnly = false;
    {
        std::lock_guard<std::mutex> lock(gate);
        if(session.revision()==revision) {
            dynamicOnly = revision == lastDynamicRevision && culture == lastCulture;
            lastAppliedGeneration=revision;
            lastCulture=culture;
            applied = true;
        }
    }
    if (applied && !dynamicOnly)
        notifyManagedApplied(current, culture, false);
}
}
