#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "experiments/csharp/NativeShooting.h"
#include "experiments/csharp/NativeTileGameplay.h"
#include "experiments/csharp/NativeWallRuntime.h"
#include "experiments/csharp/NativeLocalizationRuntime.h"
#include "content/recipes/RecipeCallbackProvider.h"
#include "experiments/csharp/WorldEntryReadiness.h"
#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/NativeAccess.h"
#include "experiments/csharp/NativeInvasionMailbox.h"
#include "experiments/csharp/NativeDustEmission.h"
#include "experiments/csharp/NativeInvasionRewards.h"
#include "experiments/csharp/NativeInvasionNotifications.h"
#include "experiments/csharp/NativeInvasionNetwork.h"
#include "experiments/csharp/NativeInvasionHud.h"
#include "experiments/csharp/ReliableNpcKill.h"
#include "experiments/csharp/NativeInvasionPersistence.h"
#include "experiments/csharp/NativeCSharpItemUse.h"
#include "experiments/csharp/WorldAuthority.h"
#include "experiments/csharp/UniversalHookMailbox.h"
#include "experiments/csharp/UniversalHooks.h"
#include "experiments/csharp/AutoSourceCache.h"
#include "experiments/csharp/AutoSourceRuntime.h"
#include "content/dusts/ModDusts.h"
#include "experiments/csharp/NativeWorldStore.h"
#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/WorldQuitSaveFence.h"
#include "experiments/csharp/NativeWorldNetwork.h"
#include "experiments/csharp/NativeWorldNetTransport.h"
#include "experiments/csharp/StableWorldUid.h"
#include "experiments/csharp/WorldNetAsync.h"
#include "il2cpp/Signature.h"
#include "experiments/csharp/NativeVisualRenderer.h"
#include "experiments/csharp/NativeRarityGameplay.h"
#include "experiments/csharp/ManagedGoreRuntime.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchEditor.h"
#include "experiments/csharp/NativeTouchStaticMappings.h"
#include "experiments/csharp/NativeTouchIcons.h"
#include "experiments/csharp/NativeTouchIconHooks.h"
#include "experiments/csharp/NativeTouchPointer.h"
#include "experiments/csharp/NativeTouchLayoutHooks.h"
#include "experiments/csharp/NativeTouchGroups.h"
#include "experiments/csharp/NativeTouchVisibility.h"
#include "experiments/csharp/NativeTouchActivation.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "graphics/CSharpScreenEffects.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeAnglerQuestGameplay.h"
#include "experiments/csharp/NativeArmorPolicies.h"
#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedSoundLoader.h"
#include "experiments/csharp/NativeCommandManifest.h"
#include "experiments/csharp/NativeCommandChatHooks.h"
#include "content/items/ModItemSave.h"
#include "content/items/ModItems.h"
#include "hook/HookManager.h"

#include "content/common/GameRefs.h"
#include "content/npcs/ModNpcs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Config.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/NpcAiHost.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/TerrariaHost.h"
#include "menu/NetRequests.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/ManagedDustBindingSelection.h"
#include "experiments/csharp/ManagedSourceClassCount.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "mods/ModLoader.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <limits.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <utility>
#include <vector>

namespace bl::csharp {
bool compiledSourceHasTouchEntry(const std::string& assembly);
worldsave::WorldNetAsync& worldNetQueue();
namespace {

constexpr const char* kFixtureId = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
constexpr const char* kCacheVersion = "BUNNY_AUTO_SOURCE_V3_NATIVE_ITEM_POLICIES_20261015";
std::chrono::steady_clock::time_point g_sourceStart{};
std::atomic<int> g_cachedMods{0};
std::atomic<int> g_compiledMods{0};
std::atomic<bool> g_runtimeWarmStarted{false};
std::atomic<bool> g_runtimeWarming{false};
std::mutex g_lock;
std::mutex g_worldTransitionMutex;
worldsave::WorldQuitSaveFence g_worldQuitSaveFence;
std::vector<AutoSourceStatus> g_statuses;
std::atomic<bool> g_started{false};
std::atomic<bool> g_initialCSharpRegistrationFinished{false};
std::atomic<bool> g_initialCSharpSummaryShown{false};
std::atomic<bool> g_preWorldBindingsArmed{false};
std::atomic<bool> g_preWorldBindingsRunning{false};
std::atomic<bool> g_delayedWorldEntry{false};
std::chrono::steady_clock::time_point g_delayedWorldSince{};
const MethodInfo* g_delayedPlayWorldMethod=nullptr;
using PlayWorldFn=void (*)(const MethodInfo*);
PlayWorldFn g_originalPlayWorld=nullptr;
std::atomic<bool> g_worldTilesLoaded{false};
std::atomic<bool> g_worldSessionReady{false};
std::atomic<bool> g_worldRegistrationReady{true};
std::atomic<int> g_worldReadyFrames{0};
std::atomic<bool> g_worldPrefixesRestored{false};
std::atomic<bool> g_worldPlayerMessagesShown{false};
std::atomic<int> g_worldMenuPollFrames{0};
std::atomic<uint64_t> g_worldEpoch{0};
std::atomic<uint64_t> g_worldDataEnteredEpoch{0};
std::atomic<int> g_worldSystemModules{0};
std::atomic<bool> g_nativeWorldSaveHooksReady{false};
std::atomic<bool> g_nativeWorldNetHooksReady{false};
std::atomic<uint64_t> g_visualUpdateTick{0};
std::atomic<uint64_t> g_liveBoundEpoch{0};
std::atomic<int> g_worldLoads{0};
std::atomic<int> g_worldExits{0};
std::atomic<int> g_worldRebindSchedules{0};
std::vector<std::pair<std::string, int>> g_pendingLiveSpecs;
std::string g_pendingLiveWork;
std::mutex g_consoleLock;
struct ConsolePending {
    std::string modId;
    std::string workDir;
    std::chrono::steady_clock::time_point started{};
    bool active = false;
};
ConsolePending g_consolePending;
std::vector<std::string> g_consoleMessages;
unsigned g_consoleRunCount = 0;

struct Curve {
    std::string modId;
    std::string name;
    std::array<int, 120> turns{};
};

struct Combat {
    std::string modId;
    std::string name;
    bool kill = false;
    SourceBoltHitPolicy hit{};
    SourceBoltKillPolicy onKill{};
};

using namespace std::chrono_literals;

std::string folderOfCheckpoint() {
    const std::string& path = config().managedCheckpointPath;
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string readText(const std::string& path, size_t limit) {
    if (path.empty()) return {};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const std::streamoff length = file.tellg();
    if (length <= 0 || static_cast<uintmax_t>(length) > limit) return {};
    std::string result(static_cast<size_t>(length), '\0');
    file.seekg(0, std::ios::beg);
    if (!file.read(result.data(), length)) return {};
    if (file.peek() != std::char_traits<char>::eof()) return {};
    return result;
}

bool writeText(const std::string& path, const std::string& value) {
    const std::string temp = path + ".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(value.data(), static_cast<std::streamsize>(value.size()));
    file.close();
    if (!file) {
        std::remove(temp.c_str());
        return false;
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

void setStatus(const AutoSourceStatus& next) {
    std::lock_guard<std::mutex> guard(g_lock);
    for (auto& entry : g_statuses) {
        if (entry.modId == next.modId) {
            entry = next;
            return;
        }
    }
    g_statuses.push_back(next);
}

void note(AutoSourceStatus* status, const std::string& phase,
          const std::string& error = {}) {
    status->phase = phase;
    status->error = error;
    setStatus(*status);
    if (!error.empty())
        BL_WARN("csharp auto %s: %s: %s", status->modId.c_str(),
                phase.c_str(), error.c_str());
}

bool safeId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char ch : id) {
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_')) return false;
    }
    return true;
}

bool hashSources(const std::string& dir, const std::string& relative,
                 PortableSha256* hash, int* count, size_t* totalBytes, int depth, bool* hasHooks = nullptr) {
    if (depth > kSourceMaxDepth || static_cast<size_t>(*count) > kSourceMaxFiles) return false;
    const std::string path = dir + (relative.empty() ? "" : "/" + relative);
    DIR* listing = opendir(path.c_str());
    if (!listing) return false;
    std::vector<std::string> names;
    while (dirent* item = readdir(listing)) {
        if (item->d_name[0] == '.') continue;
        names.emplace_back(item->d_name);
    }
    closedir(listing);
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        const std::string rel = relative.empty() ? name : relative + "/" + name;
        const std::string full = dir + "/" + rel;
        struct stat metadata{};
        if (lstat(full.c_str(), &metadata) != 0) return false;
        if (S_ISDIR(metadata.st_mode)) {
            if (!hashSources(dir, rel, hash, count, totalBytes, depth + 1, hasHooks)) return false;
        } else if (S_ISREG(metadata.st_mode) &&
                   ((name.size() > 3 && name.substr(name.size() - 3) == ".cs") ||
                    (relative == "Localization" && name.size() > 5 &&
                     name.substr(name.size() - 5) == ".json"))) {
            const bool sourceFile=name.size()>3 && name.substr(name.size()-3)==".cs";
            if (metadata.st_size < 1 || metadata.st_size > static_cast<off_t>(kSourceMaxFileBytes))
                return false;
            if (sourceFile && static_cast<size_t>(++*count) > kSourceMaxFiles) return false;
            *totalBytes+=static_cast<size_t>(metadata.st_size);
            if(*totalBytes>kSourceMaxTotalBytes)return false;
            std::string source = readText(full, kSourceMaxFileBytes);
            if (source.empty()) return false;
            if (sourceFile && hasHooks && (source.find("BunnyHookModule") != std::string::npos ||
                source.find("BunnyWorldUpdateLoader") != std::string::npos)) *hasHooks = true;
            hash->field(rel);
            hash->field(source);
        }
    }
    return true;
}

std::string fingerprint(const std::string& id, const std::string& root,
                        const std::string& runtime, bool* hasHooks = nullptr) {
    PortableSha256 hash;
    hash.field(auto_cache::envelope);
    hash.field(kCacheVersion);
    hash.field(runtime);
    hash.field(id);
    int count = 0;
    size_t totalBytes=0;
    if (!hashSources(root + "/content", "", &hash, &count, &totalBytes, 0, hasHooks) || count < 1)
        return {};
    return hash.finish();
}

bool readCache(const std::string& path, const std::string& id,
               const std::string& key, const std::string& assembly,
               std::string* defaults, std::string* curves, std::string* combat,
               std::string* reason=nullptr) {
    const std::string value = readText(path, kSourceDefaultsBytes+2*kSourceMotionBytes+2048);
    if (value.empty()) {
        if(reason)*reason="CacheFileMissingOrUnreadable";
        return false;
    }
    const auto dllHash=auto_cache::fileHash(assembly,kSourceMaxAssemblyBytes);
    if(dllHash.empty()) {
        if(reason)*reason="CompiledAssemblyMissing";
        return false;
    }
    return auto_cache::decode(value,kCacheVersion,id,key,dllHash,defaults,curves,combat,reason);
}

bool saveCache(const std::string& path, const std::string& id,
               const std::string& key, const std::string& assembly,
               const std::string& defaults, const std::string& curves,
               const std::string& combat) {
    const auto dllHash=auto_cache::fileHash(assembly,kSourceMaxAssemblyBytes);
    const auto value=auto_cache::encode(kCacheVersion,id,key,dllHash,defaults,curves,combat);
    if (value.empty()) return false;
    return writeText(path,value);
}

bool validCompileStatus(const std::string& status, const std::string& id,
                        const SourceModStatus& mod) {
    return managedSourceCompileFinished(status, id, mod);
}

bool compile(const std::string& root, const std::string& id,
             const SourceModStatus& mod, const std::string& workDir,
             std::string* defaults, std::string* curves,
             std::string* combat, std::string* error) {
    std::remove((workDir + "source-compile-status-" + id + ".txt").c_str());
    std::remove((workDir + "source-compile-status.txt").c_str());
    std::remove((workDir + "source-runtime-values.txt").c_str());
    std::remove((workDir + "source-projectile-turns.txt").c_str());
    std::remove((workDir + "source-combat-policies.txt").c_str());
    if (!requestIsolatedMonoCompile(root, id, *error)) return false;
    const std::string path = workDir + "source-compile-status-" + id + ".txt";
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::seconds(180);
    for (; std::chrono::steady_clock::now() < deadline;) {
        const std::string status = readText(path, 8192);
        if (status.find("state=failed\n") != std::string::npos &&
            status.find("modId=" + id + "\n") != std::string::npos) {
            const auto valueOf = [&](const std::string& key) {
                const size_t found = status.find("\n" + key + "=");
                if (found == std::string::npos) return std::string{};
                const size_t begin = found + key.size() + 2;
                const size_t end = status.find('\n', begin);
                return status.substr(begin, std::min<size_t>(
                    end == std::string::npos ? status.size() - begin : end - begin, 180));
            };
            *error = "Roslyn/MonoVM: " + valueOf("phase");
            const auto detail = valueOf("error");
            if (!detail.empty()) *error += " " + detail;
            const std::string diagnostic = readText(
                workDir + "source-compile-diagnostics-" + id + ".txt", 16384);
            if (!diagnostic.empty()) {
                std::string excerpt = diagnostic.substr(0, 700);
                std::replace(excerpt.begin(), excerpt.end(), '\n', ' ');
                *error += " | " + excerpt;
            }
            if (error->size() < 18)
                *error = "Roslyn/MonoVM falhou no mod " + id;
            return false;
        }
        if (status.find("state=finished\n") != std::string::npos &&
            status.find("modId=" + id + "\n") != std::string::npos &&
            !validCompileStatus(status, id, mod)) {
            const int expected = expectedManagedSourceTypes(mod);
            std::string actual = "desconhecido";
            const std::string marker = "executedTypes=";
            const size_t start = status.find(marker);
            if (start != std::string::npos) {
                const size_t value = start + marker.size();
                const size_t end = status.find('\n', value);
                const std::string raw = status.substr(value, end == std::string::npos ?
                                                        16 : std::min<size_t>(end - value, 16));
                if (!raw.empty() &&
                    std::all_of(raw.begin(), raw.end(), [](unsigned char c) {
                        return c >= '0' && c <= '9';
                    })) actual = raw;
            }
            *error = "Compilacao C# concluida, mas contrato de classes divergente: esperado " +
                     std::to_string(expected) + ", compilado " + actual +
                     "; verifique sourceCompileStatus";
            return false;
        }
        if (!validCompileStatus(status, id, mod)) {
            const auto elapsed = std::chrono::steady_clock::now() - started;
            std::this_thread::sleep_for(elapsed < 2500ms ? 35ms :
                                        elapsed < 12s ? 100ms : 300ms);
            continue;
        }
        const auto compileTiming=[&](const char* name) {
            const std::string prefix=std::string(name)+"=";
            const size_t start=status.find(prefix);
            if(start==std::string::npos)return std::string("n/a");
            const auto offset=start+prefix.size();
            const auto end=status.find('\n',offset);
            return status.substr(offset,end==std::string::npos?24:
                std::min<size_t>(24,end-offset));
        };
        BL_INFO("[CSharp][Startup] Mod=%s Stage=MonoWorkerTimeline FrameworkMs=%s ManagedCompileMs=%s WorkerTotalMs=%s FrameworkCache=%s ResidentReuse=%s MonoProbeMs=%s RoslynInvokeMs=%s VerifyInvokeMs=%s PrefetchReused=%s",
                id.c_str(),compileTiming("frameworkSetupMs").c_str(),
                compileTiming("managedCompileMs").c_str(),
                compileTiming("workerTotalMs").c_str(),
                compileTiming("frameworkCache").c_str(),
                compileTiming("residentReuse").c_str(),
                compileTiming("monoProbeMs").c_str(),
                compileTiming("roslynInvokeMs").c_str(),
                compileTiming("verifyInvokeMs").c_str(),
                compileTiming("prefetchReused").c_str());
        *defaults = readText(workDir + "source-runtime-values.txt", kSourceDefaultsBytes);
        *curves = readText(workDir + "source-projectile-turns.txt", kSourceMotionBytes);
        *combat = readText(workDir + "source-combat-policies.txt", kSourceMotionBytes);
        if (defaults->rfind("BUNNY_MANAGED_DEFAULTS_V2\n", 0) != 0 ||
            combat->rfind("BUNNY_COMBAT_POLICIES_V1\n", 0) != 0) {
            *error = "Resultado C# sem tabela valida de SetDefaults";
            return false;
        }
        return true;
    }
    *error = "Timeout de 180 segundos compilando o mod C#";
    return false;
}

bool parseCurves(const std::string& id, const std::string& input,
                 const SourceModStatus& mod, std::vector<Curve>* output) {
    if (input.empty()) return true;
    std::istringstream lines(input);
    std::string head, line;
    if (!std::getline(lines, head) || head != "BUNNY_PROJECTILE_TURNS_V1")
        return false;
    int count = 0;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        if (static_cast<size_t>(++count) > kSourceMaxCategory || line.size() > 1300) return false;
        std::istringstream fields(line);
        std::string name;
        if (!std::getline(fields, name, '\t') ||
            !mod.registeredTypes.contains(name)) return false;
        Curve curve;
        curve.modId = id;
        curve.name = name;
        for (int i = 0; i < 120; ++i) {
            std::string number;
            if (!std::getline(fields, number, '\t') || number.empty() ||
                number.size() > 5) return false;
            char* end = nullptr;
            const long angle = std::strtol(number.c_str(), &end, 10);
            if (!end || *end || angle < -80 || angle > 80) return false;
            curve.turns[static_cast<size_t>(i)] = static_cast<int>(angle);
        }
        std::string extra;
        if (std::getline(fields, extra, '\t')) return false;
        output->push_back(std::move(curve));
    }
    return true;
}

bool parseCombat(const std::string& id, const std::string& input,
                 const SourceModStatus& mod, std::vector<Combat>* output) {
    std::istringstream stream(input);
    std::string header, line;
    if (!std::getline(stream, header) || header != "BUNNY_COMBAT_POLICIES_V1")
        return false;
    std::set<std::string> names;
    int total = 0;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (static_cast<size_t>(++total) > kSourceMaxCategory || line.size() > 3200) return false;
        std::istringstream row(line);
        std::vector<std::string> parts;
        std::string part;
        while (std::getline(row, part, '\t')) parts.push_back(part);
        if (parts.size() < 3 || !mod.registeredTypes.contains(parts[1]) ||
            !names.insert(parts[0] + ":" + parts[1]).second)
            return false;
        const bool hit = parts[0] == "HIT";
        const bool kill = parts[0] == "KILL";
        if ((!hit && !kill) || parts.size() != (hit ? 259u : 515u) ||
            parts[2] != "24") return false;
        Combat entry;
        entry.modId = id;
        entry.name = parts[1];
        entry.kill = kill;
        entry.hit.buffId = 24;
        entry.onKill.buffId = 24;
        auto parse = [](const std::string& field, int low, int high, int* dest) {
            if (field.empty() || field.size() > 5) return false;
            errno = 0;
            char* end = nullptr;
            long value = std::strtol(field.c_str(), &end, 10);
            if (errno || end == field.c_str() || *end || value < low || value > high)
                return false;
            *dest = static_cast<int>(value);
            return true;
        };
        for (size_t i = 0; i < 256; ++i) {
            if (hit) {
                if (!parse(parts[3 + i], 30, 180, &entry.hit.durations[i]))
                    return false;
            } else {
                if (!parse(parts[3 + i], 24, 120, &entry.onKill.radii[i]) ||
                    !parse(parts[259 + i], 30, 180, &entry.onKill.durations[i]))
                    return false;
            }
        }
        output->push_back(std::move(entry));
    }
    return true;
}

struct ActivationContext {
    int netMode = -1;
    int gameMenu = -1;
    bool worldReady = false;
};

bool worldPlayerReady() {
    struct Refs {
        bool initialized = false;
        FieldInfo* world = nullptr;
        FieldInfo* players = nullptr;
        FieldInfo* netMode = nullptr;
        const MethodInfo* myPlayer = nullptr;
        int32_t active = -1;
    };
    static Refs refs;
    auto& api = il2cpp::api();
    if (!refs.initialized) {
        Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
        Il2CppClass* player = il2cpp::findClass({"Terraria", "Player", {}});
        refs.world = main ? il2cpp::findField(main, "ActiveWorldFileData") : nullptr;
        refs.players = main ? il2cpp::findField(main, "player") : nullptr;
        refs.netMode = main ? il2cpp::findField(main, "netMode") : nullptr;
        refs.myPlayer = main ? api.class_get_method_from_name(main, "get_myPlayer", 0) : nullptr;
        if (player) {
            FieldInfo* active = il2cpp::findField(player, "active");
            if (active) refs.active = static_cast<int32_t>(api.field_get_offset(active));
        }
        refs.initialized = true;
        if (!refs.world || !refs.players || !refs.netMode ||
            !refs.myPlayer || refs.active < 0)
            BL_WARN("csharp auto live: referencias de mundo/jogador incompletas");
    }
    if (!refs.world || !refs.players || !refs.netMode ||
        !refs.myPlayer || refs.active < 0)
        return false;
    int mode = -1;
    api.field_static_get_value(refs.netMode, &mode);
    if (mode < 0 || mode > 3) return false;
    Il2CppObject* world = nullptr;
    api.field_static_get_value(refs.world, &world);
    if (!world) return false;
    if (mode == 2) return true;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(refs.players, &players);
    if (!players) return false;
    Il2CppObject* exception = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(
        refs.myPlayer, nullptr, nullptr, &exception);
    if (exception || !boxed) return false;
    const int index = *reinterpret_cast<int*>(
        reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject));
    if (index < 0 || static_cast<uintptr_t>(index) >= players->length)
        return false;
    Il2CppObject* selected = static_cast<Il2CppObject**>(
        arrayData(players))[index];
    return selected && bl::runtime::field<uint8_t>(
        selected, refs.active) != 0;
}

ActivationContext readActivationContext() {
    ActivationContext result;
    struct Metadata {
        FieldInfo* netMode = nullptr;
        FieldInfo* menu = nullptr;
        const MethodInfo* getter = nullptr;
    };
    static Metadata storage;
    static std::atomic<const Metadata*> published{nullptr};
    static std::mutex metadataMutex;
    const Metadata* metadata=published.load(std::memory_order_acquire);
    if(!metadata) {
        std::lock_guard<std::mutex> guard(metadataMutex);
        metadata=published.load(std::memory_order_relaxed);
        if(!metadata) {
            Il2CppClass* main=il2cpp::findClass({"Terraria", "Main", {}});
            if(!main)return result;
            Metadata resolved;
            resolved.netMode=il2cpp::findField(main,"netMode");
            resolved.menu=il2cpp::findField(main,"gameMenu");
            resolved.getter=il2cpp::api().class_get_method_from_name(main,"get_gameMenu",0);
            if(!resolved.netMode||(!resolved.menu&&!resolved.getter))return result;
            // Metadata remains valid for the lifetime of the IL2CPP domain.
            storage=resolved;
            published.store(&storage,std::memory_order_release);
            metadata=&storage;
        }
    }
    FieldInfo* netMode=metadata->netMode;
    FieldInfo* menu=metadata->menu;
    auto& api = il2cpp::api();
    if (netMode)
        api.field_static_get_value(netMode, &result.netMode);
    {
        const MethodInfo* getter = metadata->getter;
        if (getter) {
            Il2CppObject* exception = nullptr;
            Il2CppObject* boxed = api.runtime_invoke(
                getter, nullptr, nullptr, &exception);
            if (!exception && boxed) {
                const auto* value = reinterpret_cast<const uint8_t*>(boxed) +
                    sizeof(Il2CppObject);
                result.gameMenu = *value ? 1 : 0;
            }
        }
    }
    if (result.gameMenu < 0 && menu) {
        uint8_t inMenu = 1;
        api.field_static_get_value(menu, &inMenu);
        result.gameMenu = inMenu ? 1 : 0;
    }
    result.worldReady = g_worldSessionReady.load(std::memory_order_acquire);
    return result;
}

bool bindLiveEventsOnWorld(const std::string& modId, int projectileType,
                           const std::string& work, uint64_t epoch, bool preWorld=false) {
    il2cpp::api().thread_attach(il2cpp::api().domain_get());
    const std::string dll = work + "compiled/" + modId + ".dll";
    const std::string probe = work + "BunnyManagedProbe.dll";
    for (int attempt = 0; attempt < 14400; ++attempt) {
        if (!preWorld && (g_worldEpoch.load(std::memory_order_acquire) != epoch ||
            !g_worldSessionReady.load(std::memory_order_acquire))) return false;
        const ActivationContext context = readActivationContext();
        if (context.netMode == 0 &&
            (preWorld || context.gameMenu == 0 ||
             (context.gameMenu < 0 && context.worldReady))) {
            {
                std::lock_guard<std::mutex> guard(g_lock);
                for (auto& state : g_statuses)
                    if (state.modId == modId)
                        state.worldReady = context.worldReady;
            }
            const auto live = managedLiveHitStatus();
            if (live.enabled && live.modId == modId) {
                if (bindManagedPlayerHurtMod(modId, dll))
                    BL_INFO("csharp BunnyPlayer Hurt: %s vinculado ao MonoVM", modId.c_str());
                requestManagedNpcBuffDispatcher();
                return true;
            }
            ManagedRuntimeStatus mono = managedRuntimeStatus();
            if ((!mono.domainInitialized || !mono.methodExecuted) &&
                !g_runtimeWarming.load(std::memory_order_acquire))
                mono = probeManagedRuntimePinnedThread(probe, "PINNED_MONO");
            if (!mono.domainInitialized || !mono.methodExecuted || mono.result != 42) {
                if (attempt == 0 || attempt == 10 || attempt == 30)
                    BL_DEBUG("csharp auto live %s: aguardando MonoVM (%s), tentativa=%d",
                            modId.c_str(), mono.phase.c_str(), attempt);
                if (attempt >= 60) return false;
                std::this_thread::sleep_for(attempt < 16 ? 75ms : 200ms);
                continue;
            }
            std::unique_lock<std::mutex> sessionGuard(g_worldTransitionMutex);
            if (!preWorld && (g_worldEpoch.load(std::memory_order_acquire) != epoch ||
                !g_worldSessionReady.load(std::memory_order_acquire))) return false;
            std::string error;
            const bool success = live.enabled
                ? enableManagedLiveMod(modId, dll, projectileType, [&] {
                      for (const auto& sourceMod : sourceModStatuses())
                          if (sourceMod.id == modId)
                              for (const auto& [name, unused] : sourceMod.registeredTypes)
                                  if (runtime::modProjectileTypeByName(modId, name) == projectileType)
                                      return name;
                      return std::string();
                  }(), &error)
                : enableManagedLiveHit(modId, dll, projectileType, &error, true);
            if (success) {
                if (bindManagedPlayerHurtMod(modId, dll))
                    BL_INFO("csharp BunnyPlayer Hurt: %s vinculado ao MonoVM", modId.c_str());
                requestManagedNpcBuffDispatcher();
                BL_INFO("csharp auto live %s: OnHitNPC/OnKill vinculados ao tipo %d%s",
                        modId.c_str(), projectileType, live.enabled ? " (mod adicional)" : "");
                const std::string root = mods::rootOf(modId);
                for (const auto& mod : sourceModStatuses()) {
                    if (mod.id != modId || !mod.loaded || !mod.runtimeApplied)
                        continue;
                    for (const auto& [name, unused] : mod.registeredTypes) {
                        const int type = runtime::modProjectileTypeByName(modId, name);
                        if (type < 0 || type == projectileType) continue;
                        const std::string eventClass = name + "LiveEvents";
                        const std::string source = readText(
                            root + "/content/Common/Live/" + eventClass + ".cs", 16384);
                        if (source.find("class " + eventClass) == std::string::npos ||
                            source.find("static int OnHitNPC(") == std::string::npos ||
                            source.find("static int OnKill(") == std::string::npos)
                            continue;
                        std::string bindingError;
                        if (enableManagedLiveProjectile(
                            modId, dll, type, name, &bindingError)) {
                            BL_INFO("csharp auto live %s: %s vinculado ao tipo %d",
                                    modId.c_str(), eventClass.c_str(), type);
                        } else {
                            BL_WARN("csharp auto live %s: %s recusado (%s)",
                                    modId.c_str(), eventClass.c_str(), bindingError.c_str());
                        }
                    }
                }
            } else {
                if (attempt == 0 || attempt == 10 || attempt == 30)
                    BL_DEBUG("csharp auto live %s: vinculacao pendente (%s), tentativa=%d",
                            modId.c_str(), error.c_str(), attempt);
                const bool temporary = error.find("ocupado") != std::string::npos ||
                    error.find("PINNED_MONO") != std::string::npos ||
                    error.find("unavailable") != std::string::npos;
                if (temporary && attempt < 60) {
                    sessionGuard.unlock();
                    std::this_thread::sleep_for(attempt < 16 ? 75ms : 200ms);
                    continue;
                }
                BL_WARN("csharp auto live %s: nao vinculado (%s)",
                        modId.c_str(), error.c_str());
            }
            return success;
        }
        if (context.netMode > 0 && context.gameMenu == 0) return false;
        if (context.netMode == 0 && context.gameMenu < 0 && attempt == 10)
            BL_WARN("csharp auto live %s: Main.gameMenu indisponivel; aguardando evento de mundo e jogador ativo",
                    modId.c_str());
        std::this_thread::sleep_for(1000ms);
    }
    return false;
}

void schedulePreWorldBindings() {
    if (!g_initialCSharpRegistrationFinished.load(std::memory_order_acquire) ||
        g_preWorldBindingsArmed.load(std::memory_order_acquire) ||
        g_preWorldBindingsRunning.exchange(true, std::memory_order_acq_rel))
        return;
    std::vector<std::pair<std::string, int>> bindings;
    std::string work;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        bindings = g_pendingLiveSpecs;
        work = g_pendingLiveWork;
    }
    if (bindings.empty()) {
        g_preWorldBindingsArmed.store(true, std::memory_order_release);
        g_preWorldBindingsRunning.store(false, std::memory_order_release);
        return;
    }
    std::thread([bindings = std::move(bindings), work] {
        bool successful = true;
        for (const auto& [id, type] : bindings) {
            if (!bindLiveEventsOnWorld(id, type, work, 0, true)) {
                BL_WARN("C# callback nao preparado antes do mundo: mod=%s tipo=%d",
                        id.c_str(), type);
                successful = false;
                break;
            }
        }
        g_preWorldBindingsArmed.store(successful, std::memory_order_release);
        g_preWorldBindingsRunning.store(false, std::memory_order_release);
    }).detach();
}

void scheduleAutoLiveBindings() {
    if (!g_worldSessionReady.load(std::memory_order_acquire)) return;
    const uint64_t epoch = g_worldEpoch.load(std::memory_order_acquire);
    if (epoch == 0) return;
    std::vector<std::pair<std::string, int>> bindings;
    std::string work;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_pendingLiveSpecs.empty() ||
            g_liveBoundEpoch.load(std::memory_order_relaxed) == epoch) return;
        g_liveBoundEpoch.store(epoch, std::memory_order_release);
        g_worldRebindSchedules.fetch_add(1, std::memory_order_relaxed);
        bindings = g_pendingLiveSpecs;
        work = g_pendingLiveWork;
    }
    std::thread([bindings = std::move(bindings), work, epoch] {
        for (const auto& [id, type] : bindings) {
            if (g_worldEpoch.load(std::memory_order_acquire) != epoch ||
                !g_worldSessionReady.load(std::memory_order_acquire)) return;
            bindLiveEventsOnWorld(id, type, work, epoch);
        }
    }).detach();
}

bool recoverRuntimeCacheIdentityAfterCompile(
    const std::string& work, const std::string& root, const std::string& id,
    const std::string& cacheDir, std::string* runtimeIdentity,
    std::string* key, std::string* cacheFile) {
    const std::string initializedIdentity = auto_cache::runtimeIdentity(work);
    if (initializedIdentity.empty() || initializedIdentity==*runtimeIdentity) return false;
    const std::string initializedKey = fingerprint(id, root, initializedIdentity);
    if (initializedKey.empty()) return false;
    *runtimeIdentity = initializedIdentity;
    *key = initializedKey;
    *cacheFile = cacheDir + "/" + id + "-" + initializedKey + ".cache";
    return true;
}

void run() {
    auto& api = il2cpp::api();
    api.thread_attach(api.domain_get());
    const std::string work = folderOfCheckpoint();
    if (work.empty()) return;
    const std::string cacheDir = work + "auto-source-cache";
    mkdir(cacheDir.c_str(), 0700);
    std::string runtimeIdentity;
    bool runtimeChecked=false;
    std::vector<Curve> pending;
    std::vector<Combat> pendingCombat;
    const auto loadedMods=sourceModStatuses();
    std::set<std::string> activeLocales;
    for (const auto& mod : loadedMods)
        if (mod.loaded && mod.sourceFiles && mod.id != kFixtureId && safeId(mod.id))
            activeLocales.insert(mod.id);
    localization::retainActiveMods(activeLocales);
    std::vector<std::pair<std::string, std::string>> pendingWorldModules;
    size_t expectedWorldModules = 0;
    for (const auto& status : loadedMods)
        if (status.loaded && status.worldSystems > 0) ++expectedWorldModules;
    if (expectedWorldModules > 0)
        g_worldRegistrationReady.store(false, std::memory_order_release);
    for (const auto& mod : loadedMods) {
        if (!mod.loaded || !mod.sourceFiles) continue;
        AutoSourceStatus status;
        status.modId = mod.id;
        if (mod.id == kFixtureId) {
            note(&status, "manual-fixture");
            continue;
        }
        if (!safeId(mod.id)) {
            note(&status, "failed", "Identidade do mod invalida");
            continue;
        }
        const auto elapsedMilliseconds=[](auto start) {
            return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-start).count());
        };
        const std::string root = mods::rootOf(mod.id);
        if (root.empty()) {
            note(&status, "failed", "Arquivos C# nao encontrados");
            continue;
        }
        if(!runtimeChecked) {
            const auto runtimeStarted=std::chrono::steady_clock::now();
            runtimeIdentity=auto_cache::runtimeIdentity(work);
            runtimeChecked=true;
            BL_INFO("[CSharp][Startup] Stage=RuntimeCacheIdentity TimeMs=%lld Cache=%s",
                    elapsedMilliseconds(runtimeStarted),runtimeIdentity.empty()?"Bypassed":"Ready");
            if(runtimeIdentity.empty())BL_WARN("csharp auto: framework incompleto; cache externo desativado neste carregamento");
        }
        const auto scanStarted = std::chrono::steady_clock::now();
        bool hasUniversalHooks = mod.hookModules > 0;
        std::string key = fingerprint(mod.id, root, runtimeIdentity, &hasUniversalHooks);
        if (key.empty()) {
            note(&status, "failed", "Arquivos C# nao encontrados");
            continue;
        }
        const auto fingerprintMs = elapsedMilliseconds(scanStarted);
        std::string defaults, curves, combat, error;
        std::string cacheFile = cacheDir + "/" + mod.id + "-" + key + ".cache";
        const auto readStarted = std::chrono::steady_clock::now();
        const std::string assembly=work+"compiled/"+mod.id+".dll";
        std::string cacheReason=runtimeIdentity.empty()?"RuntimeIdentityUnavailable":"NotChecked";
        status.cached = !runtimeIdentity.empty() &&
            readCache(cacheFile, mod.id, key, assembly, &defaults, &curves, &combat, &cacheReason);
        if (status.cached && mod.commands > 0) {
            std::vector<NativeCommandSpec> cachedCommands;
            std::string commandError;
            if (!parseNativeCommandManifest(
                    readText(assembly + ".commands", 96 * 1024), assembly,
                    mod.commandClasses, &cachedCommands, &commandError)) {
                status.cached = false;
                cacheReason = "CommandManifest:" + commandError;
            }
        }
        std::string cacheWrite=status.cached?"NotNeeded":"Unavailable";
        const auto cacheMs = fingerprintMs + elapsedMilliseconds(readStarted);
        if (!status.cached) {
            BL_INFO("[CSharp][Startup] Mod=%s Stage=EarlyCompile Reason=CacheMiss BeforeNativeContent=1",mod.id.c_str());
        }
        long long compileMs=0;
        if (!status.cached) {
            note(&status, "compiling");
            const auto compileStarted=std::chrono::steady_clock::now();
            const bool compiled=compile(root, mod.id, mod, work, &defaults, &curves, &combat, &error);
            compileMs=elapsedMilliseconds(compileStarted);
            if (!compiled) {
                note(&status, "failed", error);
                continue;
            }
        }
        note(&status, "waiting-content");
        bool ready = false;
        const auto tablesStarted = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - tablesStarted < 90s) {
            if (runtime::modProjectilesSettled()) {
                ready = true;
                break;
            }
            const auto elapsed = std::chrono::steady_clock::now() - tablesStarted;
            std::this_thread::sleep_for(elapsed < 2s ? 35ms : 150ms);
        }
        const auto tablesMs = elapsedMilliseconds(tablesStarted);
        if (!ready) {
            note(&status, "failed", "Tabelas de projetil nao ficaram prontas");
            continue;
        }
        if ((mod.projectiles > 0 || mod.npcs > 0 || mod.worldSystems > 0 || hasUniversalHooks || mod.screenEffects > 0 || mod.commands > 0 ||
             (defaults.find("LOCALE\t") != std::string::npos ||
              defaults.find("LOCALE_MODULES\t") != std::string::npos ||
              defaults.find("LOCALE_RUNTIME\t") != std::string::npos)) &&
            !g_runtimeWarmStarted.exchange(true, std::memory_order_acq_rel)) {
            const std::string probe = work + "BunnyManagedProbe.dll";
            struct stat probeStat{};
            if (stat(probe.c_str(), &probeStat) == 0 && S_ISREG(probeStat.st_mode)) {
                g_runtimeWarming.store(true, std::memory_order_release);
                std::thread([probe] {
                    const ManagedRuntimeStatus state =
                        probeManagedRuntimePinnedThread(probe, "PINNED_MONO");
                    if (!state.methodExecuted || state.result != 42)
                        BL_DEBUG("csharp startup: MonoVM prewarm pendente (%s)",
                                 state.phase.c_str());
                    g_runtimeWarming.store(false, std::memory_order_release);
                }).detach();
            }
        }
        if (!status.cached && recoverRuntimeCacheIdentityAfterCompile(
                work, root, mod.id, cacheDir, &runtimeIdentity, &key, &cacheFile))
            BL_INFO("[CSharp][Startup] Stage=RuntimeCacheIdentity Cache=ChangedAfterCompile");
        if (!status.cached && !runtimeIdentity.empty()) {
            cacheWrite=saveCache(cacheFile, mod.id, key, assembly, defaults, curves, combat)
                ?"Saved":"Failed";
            if(cacheWrite=="Failed")
                BL_WARN("csharp auto %s: cache verificado nao foi gravado", mod.id.c_str());
        }
        note(&status, "applying");
        const auto applyStarted=std::chrono::steady_clock::now();
        bool defaultsApplied = applyManagedSourceDefaults(mod.id, defaults, &error);
        if (!defaultsApplied && status.cached &&
            error.rfind("Linha V2 inesperada", 0) == 0) {
            BL_WARN("csharp auto %s: cache V2 incompativel (%s); recompilando",mod.id.c_str(),error.c_str());
            status.cached=false;note(&status,"compiling");
            const auto recoveryStarted=std::chrono::steady_clock::now();
            const bool rebuilt=compile(root,mod.id,mod,work,&defaults,&curves,&combat,&error);
            compileMs+=elapsedMilliseconds(recoveryStarted);
            if (rebuilt) {
                if(!runtimeIdentity.empty()&&
                    !saveCache(cacheFile,mod.id,key,assembly,defaults,curves,combat))
                    BL_WARN("csharp auto %s: cache recompilado nao foi gravado",mod.id.c_str());
                note(&status,"applying");
                defaultsApplied=applyManagedSourceDefaults(mod.id,defaults,&error);
            }
        }
        if (defaultsApplied) {
            if (compiledSourceHasTouchEntry(assembly)) {
                registerManagedTouchEntry(mod.id, assembly);
                if (!g_runtimeWarmStarted.exchange(true, std::memory_order_acq_rel)) {
                    const auto probe = work + "BunnyManagedProbe.dll";
                    g_runtimeWarming.store(true, std::memory_order_release);
                    std::thread([probe] {
                        const auto state = probeManagedRuntimePinnedThread(
                            probe, "PINNED_MONO");
                        if (!state.methodExecuted || state.result != 42)
                            BL_ERROR("[CSharp][Touch] Managed worker unavailable phase=%s",
                                     state.phase.c_str());
                        g_runtimeWarming.store(false, std::memory_order_release);
                    }).detach();
                }
            }
            bindNativeRecipeAssembly(mod.id,assembly);
            bindNativeTileAssembly(mod.id,assembly);
            if(!shooting::bindAssembly(mod.id,assembly,&error))defaultsApplied=false;
            if (defaultsApplied && (defaults.find("LOCALE\t") != std::string::npos ||
              defaults.find("LOCALE_MODULES\t") != std::string::npos ||
              defaults.find("LOCALE_RUNTIME\t") != std::string::npos))
                localization::bindManagedAssembly(mod.id, assembly);
            static const runtime::RecipeCallbackProvider provider{prepareNativeRecipeCallbackProvider,trackNativeRecipeCallbacks};
            runtime::setRecipeCallbackProvider(&provider);
            if((hasManagedRecipeCallbacks(mod.id)||hasManagedAnglerQuestCallbacks(mod.id)||shooting::hasPolicies(mod.id)||mod.tiles>0||mod.rarities>0)&&!g_runtimeWarmStarted.exchange(true,std::memory_order_acq_rel)) {
                const auto probe=work+"BunnyManagedProbe.dll";
                g_runtimeWarming.store(true,std::memory_order_release);
                std::thread([probe]{
                    const auto state=probeManagedRuntimePinnedThread(probe,"PINNED_MONO");
                    if(!state.methodExecuted||state.result!=42)
                        BL_ERROR("[CSharp][ERROR][RecipeCallback] Stage=InitializeMonoWorker Probe=%s Result=Pending Phase=%s Detail=%s",probe.c_str(),state.phase.c_str(),state.error.c_str());
                    g_runtimeWarming.store(false,std::memory_order_release);
                }).detach();
            }
        }
        if (defaultsApplied && mod.rarities > 0) {
            std::string identities;
            int count = 0;
            for (const auto& [key, type] : mod.registeredKindTypes) {
                if (!key.starts_with("RARITY:")) continue;
                if (type < 12 || type >= 4108) {
                    error = "[CSharp][ERROR][RarityLoader] Mod=" + mod.id +
                        " Stage=Warmup Reason=InvalidNativeRarityId";
                    defaultsApplied = false;
                    break;
                }
                if (!identities.empty()) identities.push_back(';');
                identities += key.substr(7) + ":" + std::to_string(type);
                ++count;
            }
            if (defaultsApplied && (count != mod.rarities || identities.size() > 32768 ||
                !queueManagedRarityWarmup(mod.id, assembly, identities) ||
                !registerNativeRaritySampling(mod.id, assembly))) {
                error = "[CSharp][ERROR][RarityLoader] Mod=" + mod.id +
                    " Stage=Warmup Reason=RarityIdentityQueueRejected";
                defaultsApplied = false;
            }
        }
        if (!defaultsApplied) {
            note(&status, "failed", error);
            continue;
        }
        BL_INFO("[CSharp][Startup] Mod=%s Stage=DefaultsRequest Cache=%s CacheReason=%s CacheWrite=%s WaitTablesMs=%lld CacheReadMs=%lld CompileMs=%lld ApplyAndRecoveryMs=%lld",
                mod.id.c_str(),status.cached?"Hit":"Miss",cacheReason.c_str(),cacheWrite.c_str(),
                tablesMs,cacheMs,compileMs,elapsedMilliseconds(applyStarted));
        status.applied = true;
        if (mod.screenEffects > 0) {
            graphics::CSharpScreenSceneCallback sceneCallback;
            if (graphics::findCSharpScreenSceneCallback(mod.id, &sceneCallback)) {
                if (registerManagedScreenSceneMod(mod.id, assembly,
                      sceneCallback.nameSpace, sceneCallback.className,
                      sceneCallback.methodName))
                    BL_INFO("CSharp ScreenFx: managed scene callback registered mod=%s",
                            mod.id.c_str());
                else
                    BL_WARN("CSharp ScreenFx: managed scene callback unavailable mod=%s, native fallback",
                            mod.id.c_str());
            }
        }
        if (hasUniversalHooks && !registerUniversalHookMod(mod.id, assembly))
            BL_WARN("csharp universal hooks: fila recusada mod=%s", mod.id.c_str());
        if (mod.dusts > 0) {
            const auto dustTypes = selectManagedDustBindings(mod);
            BL_INFO("[CSharp][DustLoader] Stage=BindingPlan Mod=%s Registered=%d ManagedHooks=%zu",
                    mod.id.c_str(), mod.dusts, dustTypes.size());
            if (!dustTypes.empty()) {
                if (!registerManagedDustSourceMod(mod.id, assembly, dustTypes)) {
                    BL_WARN("[CSharp][DustLoader] Mod=%s Stage=MonoRegister Result=Rejected",
                            mod.id.c_str());
                } else {
                    const std::string probe = work + "BunnyManagedProbe.dll";
                    struct stat probeStat{};
                    if (stat(probe.c_str(), &probeStat) != 0 || !S_ISREG(probeStat.st_mode)) {
                        BL_ERROR("[CSharp][DustLoader] Stage=MonoWorkerPrewarm Reason=ProbeMissing");
                    } else if (!g_runtimeWarmStarted.exchange(true, std::memory_order_acq_rel)) {
                        g_runtimeWarming.store(true, std::memory_order_release);
                        std::thread([probe] {
                            const auto state = probeManagedRuntimePinnedThread(probe, "PINNED_MONO");
                            if (!state.methodExecuted || state.result != 42)
                                BL_ERROR("[CSharp][DustLoader] Stage=MonoWorkerPrewarm Result=Pending Phase=%s",
                                         state.phase.c_str());
                            g_runtimeWarming.store(false, std::memory_order_release);
                        }).detach();
                    }
                }
            }
        }
        if (mod.worldSystems > 0) {
            g_worldRegistrationReady.store(false, std::memory_order_release);
            pendingWorldModules.emplace_back(mod.id, assembly);
        }
        if (mod.npcs > 0 || mod.projectiles > 0 || mod.items > 0) {
            const auto npcs = runtime::registeredNpcsForMod(mod.id);
            const auto projectiles = runtime::registeredProjectilesForMod(mod.id);
            if (registerManagedEntitySourceMod(mod.id, assembly, npcs, projectiles))
                BL_INFO("csharp source entities: mod=%s registered npc=%zu projectile=%zu",
                        mod.id.c_str(), npcs.size(), projectiles.size());
        }
        if (status.cached) g_cachedMods.fetch_add(1, std::memory_order_relaxed);
        else g_compiledMods.fetch_add(1, std::memory_order_relaxed);
        g_worldPrefixesRestored.store(false, std::memory_order_release);
        std::vector<Curve> entries;
        if (!parseCurves(mod.id, curves, mod, &entries)) {
            note(&status, "failed", "Curvas de projeteis C# invalidas");
            continue;
        }
        std::vector<Combat> effects;
        if (!parseCombat(mod.id, combat, mod, &effects)) {
            note(&status, "failed", "Tabela automatica OnHitNPC/OnKill invalida");
            continue;
        }
        if (mod.commands > 0) {
            std::vector<NativeCommandSpec> commandSpecs;
            std::string commandError;
            if (!parseNativeCommandManifest(
                    readText(assembly + ".commands", 96 * 1024), assembly,
                    mod.commandClasses, &commandSpecs, &commandError) ||
                !nativeCommandMailbox().registerMod(mod.id, commandSpecs, &commandError, false)) {
                note(&status, "failed", "ModCommand: " + commandError);
                continue;
            }
            if (!bindManagedCommandMod(mod.id, assembly, &commandError)) {
                nativeCommandMailbox().unregisterMod(mod.id);
                note(&status, "failed", "ModCommand: " + commandError);
                continue;
            }
            BL_INFO("[CSharp][CommandLoader] Mod=%s Stage=Verified Commands=%d Names=%zu",
                    mod.id.c_str(), mod.commands, commandSpecs.size());
        }
        status.curves = static_cast<int>(entries.size());
        for (const auto& effect : effects) {
            if (effect.kill) ++status.killPolicies;
            else ++status.hitPolicies;
        }
        pending.insert(pending.end(), entries.begin(), entries.end());
        pendingCombat.insert(pendingCombat.end(), effects.begin(), effects.end());
        note(&status, entries.empty() && effects.empty() ? "ready" : "waiting-world");
    }
    bool worldModulesReady = pendingWorldModules.size() == expectedWorldModules;
    for (const auto& [id, assembly] : pendingWorldModules) {
        worldsave::MonoWorldRequest request{};
        request.action = worldsave::MonoWorldAction::Register;
        request.modUid = worldsave::stableWorldSystemUid(id);
        request.assemblyPath = assembly;
        std::string lastReason = "MonoVM registration unavailable";
        bool registered = false;
        for (int attempt = 0; attempt < 30; ++attempt) {
            if (!managedRuntimeStatus().methodExecuted) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            worldsave::MonoWorldReply reply;
            const bool arrived = requestManagedWorldOnMono(request, &reply, 1500);
            if (arrived && reply.success) {
                registered = true;
                break;
            }
            lastReason = arrived ? reply.error : "MonoVM world registration deadline";
            if (arrived && reply.error.find("MonoVM not ready") == std::string::npos)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!registered) {
            worldModulesReady = false;
            std::lock_guard<std::mutex> guard(g_lock);
            for (auto& status : g_statuses) {
                if (status.modId == id) {
                    status.phase = "failed";
                    status.error = "WorldSystem registration: " + lastReason;
                    break;
                }
            }
            BL_WARN("csharp WorldSystem %s: registro bloqueado (%s)",
                    id.c_str(), lastReason.c_str());
        }
    }
    g_worldSystemModules.store(worldModulesReady
        ? static_cast<int>(pendingWorldModules.size()) : 0, std::memory_order_release);
    g_worldRegistrationReady.store(worldModulesReady, std::memory_order_release);
    if (pending.empty() && pendingCombat.empty()) {
        g_initialCSharpRegistrationFinished.store(true, std::memory_order_release);
        schedulePreWorldBindings();
        return;
    }
    const ActivationContext context = readActivationContext();
    BL_INFO("csharp auto: preparando curvas C#, netMode=%d gameMenu=%d (opcional)",
            context.netMode, context.gameMenu);
    for (const auto& curve : pending) {
        std::string error;
        bool enabled = false;
        if (context.netMode < 0) {
            error = "Terraria.Main.netMode indisponivel";
        } else if (context.netMode != 0) {
            error = "IA automatica C# requer singleplayer";
        } else {
            enabled = enableManagedProjectileCurve(
                curve.modId, curve.name, curve.turns, &error, true);
        }
        std::lock_guard<std::mutex> lock(g_lock);
        for (auto& state : g_statuses) {
            if (state.modId != curve.modId) continue;
            state.netMode = context.netMode;
            state.gameMenu = context.gameMenu;
            if (enabled) {
                ++state.enabledCurves;
                state.phase = state.enabledCurves == state.curves &&
                    state.enabledHits == state.hitPolicies &&
                    state.enabledKills == state.killPolicies ? "ready" : "enabling";
            } else {
                state.phase = context.netMode > 0 ? "singleplayer-only" : "failed";
                state.error = error;
            }
        }
        if (!enabled)
            BL_WARN("csharp auto: curva %s/%s recusada: %s",
                    curve.modId.c_str(), curve.name.c_str(), error.c_str());
    }
    for (const auto& effect : pendingCombat) {
        std::string error;
        bool enabled = false;
        if (context.netMode != 0) {
            error = "Efeitos C# automaticos requerem singleplayer";
        } else if (effect.kill) {
            enabled = enableAutoProjectileOnKill(
                effect.modId, effect.name, effect.onKill, &error);
        } else {
            enabled = enableAutoProjectileOnHit(
                effect.modId, effect.name, effect.hit, &error);
        }
        std::lock_guard<std::mutex> guard(g_lock);
        for (auto& status : g_statuses) {
            if (status.modId != effect.modId) continue;
            status.netMode = context.netMode;
            status.gameMenu = context.gameMenu;
            if (enabled) {
                if (effect.kill) ++status.enabledKills;
                else ++status.enabledHits;
                if (status.phase != "failed")
                    status.phase = status.enabledCurves == status.curves &&
                        status.enabledHits == status.hitPolicies &&
                        status.enabledKills == status.killPolicies ? "ready" : "enabling";
            } else {
                status.phase = context.netMode > 0 ? "singleplayer-only" : "failed";
                status.error = error;
            }
        }
        if (!enabled)
            BL_WARN("csharp auto: efeito %s/%s recusado: %s",
                    effect.modId.c_str(), effect.name.c_str(), error.c_str());
    }
    BL_INFO("csharp auto: curvas e efeitos OnHitNPC/OnKill registrados");
    std::set<std::string> liveMods;
    std::vector<std::pair<std::string, int>> bindings;
    for (const auto& effect : pendingCombat) {
        if (effect.kill || liveMods.contains(effect.modId)) continue;
        const std::string root = mods::rootOf(effect.modId);
        if (root.empty()) continue;
        const std::string declared = readText(
            root + "/content/Common/Live/BunnyLiveEvents.cs", 16384);
        if (declared.find("class BunnyLiveEvents") == std::string::npos ||
            declared.find("static int OnHitNPC(") == std::string::npos ||
            declared.find("static int OnKill(") == std::string::npos)
            continue;
        const auto hit = autoProjectileHitStatus(effect.modId, effect.name);
        const auto kill = autoProjectileKillStatus(effect.modId, effect.name);
        if (!hit.registered || !hit.enabled || !hit.installed ||
            !kill.registered || !kill.enabled || !kill.installed ||
            hit.type < 0 || hit.type != kill.type) continue;
        liveMods.insert(effect.modId);
        bindings.emplace_back(effect.modId, hit.type);
    }
    if (!bindings.empty()) {
        {
            std::lock_guard<std::mutex> guard(g_lock);
            g_pendingLiveSpecs = std::move(bindings);
            g_pendingLiveWork = work;
        }
    }
    g_initialCSharpRegistrationFinished.store(true, std::memory_order_release);
    schedulePreWorldBindings();
}

}

bool submitInteractiveConsoleSource(const std::string& id, const std::string& root,
                                    const std::string& workDir, std::string* error) {
    if (!error) return false;
    error->clear();
    if (id.size()!=36 || id.find_first_not_of("0123456789abcdef-")!=std::string::npos ||
        root.size()>2048 || workDir.size()>2048 || root.find("..")!=std::string::npos) {
        *error="Identidade ou diretorio C# invalido";
        return false;
    }
    char checkedRoot[PATH_MAX]{},checkedWork[PATH_MAX]{},checkedCheckpoint[PATH_MAX]{};
    if (!realpath(root.c_str(),checkedRoot) ||
        !realpath(workDir.c_str(),checkedWork) ||
        !realpath(folderOfCheckpoint().c_str(),checkedCheckpoint) ||
        std::string(checkedWork)!=std::string(checkedCheckpoint) ||
        std::string(checkedRoot)!=std::string(checkedWork)+"/interactive/"+id) {
        *error="Diretorio privado do console C# nao autorizado";
        return false;
    }
    struct stat source{};
    if(stat((root+"/content/ConsoleEntry.cs").c_str(),&source)!=0 ||
       !S_ISREG(source.st_mode)||source.st_size<64||source.st_size>65536) {
       *error="Fonte temporaria C# invalida";return false;
    }
    if (!g_initialCSharpRegistrationFinished.load(std::memory_order_acquire) ||
        !managedRuntimeStatus().domainInitialized) {
        *error="Aguarde o MonoVM e os mods C# carregarem";
        return false;
    }
    std::lock_guard<std::mutex> guard(g_consoleLock);
    if (g_consolePending.active) {
        *error="Aguarde a compilacao C# anterior";
        return false;
    }
    if (g_consoleRunCount>=24) {
        *error="Limite de 24 execucoes C# atingido; reinicie o Bunny Loader";
        return false;
    }
    const std::string status=readText(workDir+"/source-compile-status.txt",8192);
    if(status.find("state=compiling\n")!=std::string::npos ||
       status.find("state=preparing-framework\n")!=std::string::npos) {
        *error="Roslyn ocupado com outro mod C#";return false;
    }
    std::remove((workDir+"/source-compile-status.txt").c_str());
    if(!requestIsolatedMonoCompile(root,id,*error))return false;
    g_consolePending={id,workDir,std::chrono::steady_clock::now(),true};
    g_consoleMessages.push_back("iC# enviado ao Roslyn isolado; aguardando compilacao...");
    return true;
}

std::vector<std::string> takeInteractiveConsoleOutput() {
    std::lock_guard<std::mutex> guard(g_consoleLock);
    std::vector<std::string> result;
    result.swap(g_consoleMessages);
    return result;
}

void pumpInteractiveConsoleOnGameThread() {
    static unsigned tick=0;
    if((++tick%10)!=0)return;
    std::string id,dir;
    {
        std::lock_guard<std::mutex> guard(g_consoleLock);
        if(!g_consolePending.active)return;
        id=g_consolePending.modId;
        dir=g_consolePending.workDir;
    }
    const std::string status=readText(dir+"/source-compile-status.txt",8192);
    if(status.find("state=finished\n")==std::string::npos &&
       status.find("state=failed\n")==std::string::npos) {
        std::lock_guard<std::mutex> guard(g_consoleLock);
        if(g_consolePending.active &&
           std::chrono::steady_clock::now()-g_consolePending.started>std::chrono::seconds(50)) {
            g_consolePending.active=false;
            g_consoleMessages.push_back("xC# timeout na compilacao Roslyn (50s)");
        }
        return;
    }
    const bool finished=status.find("state=finished\n")!=std::string::npos &&
        status.find("modId="+id+"\n")!=std::string::npos &&
        status.find("assemblyReady=1\n")!=std::string::npos;
    std::string output,error;
    if(finished){
        if(!invokeInteractiveCSharp(dir+"/compiled/"+id+".dll",&output,&error))
            output.clear();
    }else if(status.find("state=failed\n")!=std::string::npos){
        error=readText(dir+"/source-compile-diagnostics.txt",8192);
        if(error.empty())error=status.substr(0,2048);
    }else return;
    {
        std::lock_guard<std::mutex> guard(g_consoleLock);
        if(!g_consolePending.active || g_consolePending.modId!=id)return;
        g_consolePending.active=false;
        ++g_consoleRunCount;
        if(error.empty())
            g_consoleMessages.push_back("r"+(output.empty()?"Executado":output));
        else g_consoleMessages.push_back("x"+error.substr(0,4000));
    }
}

void reportCSharpLoadSummary() {
    if (!g_initialCSharpRegistrationFinished.load(std::memory_order_acquire) ||
        g_initialCSharpSummaryShown.load(std::memory_order_acquire)) return;

    const auto statuses = autoSourceStatuses();
    if (statuses.empty()) return;
    const auto basic = basicItemLoadStatus();
    int successful = 0, failed = 0, pendingCallbacks = 0;
    for (const auto& status : statuses) {
        if (status.phase == "failed" || !status.error.empty()) ++failed;
        else if (!status.applied) ++pendingCallbacks;
        else {
            ++successful;
            if (status.enabledCurves < status.curves ||
                status.enabledHits < status.hitPolicies ||
                status.enabledKills < status.killPolicies) ++pendingCallbacks;
        }
    }
    if (!g_preWorldBindingsArmed.load(std::memory_order_acquire))
        ++pendingCallbacks;
    else {
        std::lock_guard<std::mutex> lock(g_lock);
        if (!g_pendingLiveSpecs.empty() &&
            !managedNpcBuffEffectStatus().gameHookInstalled) ++pendingCallbacks;
    }
    const auto wallStatus=walls::loadStatus();
    failed+=wallStatus.failed;
    const int pending = wallStatus.pending + basic.pendingBehaviors + basic.pendingRecipes +
        basic.pendingPlacements + pendingCallbacks + static_cast<int>(pendingNativeVisualRenderer());
    if (pending) return;
    if (g_initialCSharpSummaryShown.exchange(true, std::memory_order_acq_rel))
        return;
    if (pending || failed)
        BL_WARN("C# carregamento parcial: mods=%d falhas=%d pendentes=%d (comportamentos=%d receitas=%d tiles=%d callbacks=%d); confira os erros do mod",
                successful, failed, pending, basic.pendingBehaviors, basic.pendingRecipes,
                basic.pendingPlacements, pendingCallbacks);
    else
        BL_INFO("C# carregou sem problemas: %d mod(s), %d item(ns), %d prefixo(s), %d receita(s), cache=%d, compilados=%d, preparo=%lldms",
                successful, basic.items, registeredManagedPrefixCount(),
                basic.installedRecipes, g_cachedMods.load(std::memory_order_relaxed),
                g_compiledMods.load(std::memory_order_relaxed),
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - g_sourceStart).count()));
}

void showWorldPlayerMessagesWhenReady() {
    if (g_worldPlayerMessagesShown.load(std::memory_order_acquire) ||
        !g_worldSessionReady.load(std::memory_order_acquire)) return;
    if (!showManagedPlayerWorldJoinMessages()) return;
    g_worldPlayerMessagesShown.store(true, std::memory_order_release);
}

void retryWorldManagedPrefixesWhenReady() {
    if (g_worldPrefixesRestored.load(std::memory_order_acquire) ||
        !g_worldSessionReady.load(std::memory_order_acquire) ||
        !managedPrefixGameplayReady()) return;
    if (g_worldPrefixesRestored.exchange(true, std::memory_order_acq_rel))
        return;
    BL_DEBUG("csharp prefix: restauracao do personagem pendente");
    bl::runtime::retryPendingManagedPrefixesOnWorldReady();
}

void noteAutoWorldTilesLoaded(const std::string& worldPath) {
    angler::invalidateWorldSession();
    std::lock_guard<std::mutex> sessionGuard(g_worldTransitionMutex);
    nativeCommandMailbox().newWorld();
    resetManagedCommandSession();
    managedSound::worldExit();
    resetNativeTileSessionState();
    gore::worldBoundary();
    touch::registry().resetWorld();
    touch::resetNativeTouchGroupSession();
    touch::invalidateNativeTouchLayouts();
    clearManagedVisualCommands();
    resetNativeVisualRenderer();
    g_visualUpdateTick.store(0, std::memory_order_relaxed);
    g_worldLoads.fetch_add(1, std::memory_order_relaxed);
    const uint64_t previousEpoch = g_worldDataEnteredEpoch.exchange(
        0, std::memory_order_acq_rel);
    if (previousEpoch != 0) {
        std::string unloadError;
        if (!worldsave::leaveManagedWorld(previousEpoch, &unloadError))
            BL_WARN("CSharp WorldSave previous world unload: %s", unloadError.c_str());
    }
    const uint64_t epoch = g_worldEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (g_worldSystemModules.load(std::memory_order_acquire) > 0)
        worldNetQueue().invalidate();
    invalidateManagedWorldMonoRequests();
    // O caminho foi extraído de Main.ActiveWorldFileData pelo hook nativo dos tiles.
    // Cloud e caminhos inválidos deixam o sidecar C# explicitamente desabilitado.
    const bool localWorld = worldsave::activeWorldStore().beginWorld(
        epoch, worldPath, worldPath.empty());
    if (!localWorld)
        BL_DEBUG("csharp world save: sidecar desativado para esta sessão");
    g_worldSessionReady.store(false, std::memory_order_release);
    setNativeInvasionWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionRewardWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionNoticeWorld(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionNetWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionHudWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    updateNativeInvasionWorldPath(false, g_worldEpoch.load(std::memory_order_acquire));
        setNativeItemUseWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    pauseManagedEntitySourceCallbacks();
    runtime::resetManagedDustLifecycle();
    resetManagedDustSourceWorld();
    pauseManagedScreenSceneCallbacks();
    resetReliableNpcKills();
    resetManagedSourceNpcSlots();
    resetManagedSourceProjectileSlots();
    g_worldReadyFrames.store(0, std::memory_order_relaxed);
    g_worldMenuPollFrames.store(0, std::memory_order_relaxed);
    if (!g_preWorldBindingsArmed.load(std::memory_order_acquire))
        g_worldPrefixesRestored.store(false, std::memory_order_release);
    g_worldPlayerMessagesShown.store(false, std::memory_order_release);
    g_worldTilesLoaded.store(true, std::memory_order_release);
    if (g_preWorldBindingsArmed.load(std::memory_order_acquire)) {
        g_liveBoundEpoch.store(g_worldEpoch.load(std::memory_order_acquire),
                               std::memory_order_release);
    } else {
        if (managedLiveHitStatus().enabled) pauseManagedLiveHit();
        pauseManagedPlayerHurtBindings();
    }
}

void flushDeferredWorldEntry();

bool compiledSourceHasTouchEntry(const std::string& assembly) {
    // Class names are stored as UTF-8 metadata in a Mono DLL.
    // This bounded one-time probe avoids loading a MonoVM for C# mods
    // which do not declare any touch input callbacks.
    struct stat info{};
    if (stat(assembly.c_str(), &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 8 * 1024 * 1024) return false;
    std::ifstream file(assembly, std::ios::binary);
    if (!file) return false;
    constexpr std::string_view token = "BunnyTouchEntry";
    std::string block;
    block.reserve(8192 + token.size());
    char buffer[8192];
    while (file) {
        file.read(buffer, sizeof(buffer));
        const std::streamsize length = file.gcount();
        if (length <= 0) break;
        block.append(buffer, static_cast<size_t>(length));
        if (block.find(token) != std::string::npos) return true;
        if (block.size() > token.size())
            block.erase(0, block.size() - token.size());
    }
    return false;
}

void noteAutoGameUpdate() {
    if (nativeCommandMailbox().hasCommands())
        installNativeCommandChatHooks();
    pumpNativeCommandRepliesOnGameThread();
    pumpNativeRarityGameplay();
    const bool invasionWorldReady = g_worldSessionReady.load(std::memory_order_acquire);
    const uint64_t invasionWorldEpoch = g_worldEpoch.load(std::memory_order_acquire);
    setNativeInvasionWorldState(invasionWorldReady, invasionWorldEpoch);
    setNativeItemUseWorldState(invasionWorldReady, invasionWorldEpoch);
    setNativeDustEmissionWorld(invasionWorldReady, invasionWorldEpoch);
    pumpNativeDustEmission();
    setNativeInvasionRewardWorldState(invasionWorldReady, invasionWorldEpoch);
    setNativeInvasionNoticeWorld(invasionWorldReady, invasionWorldEpoch);
    setNativeInvasionNetWorldState(invasionWorldReady, invasionWorldEpoch);
    setNativeInvasionHudWorldState(invasionWorldReady, invasionWorldEpoch);
    pumpNativeInvasionSpawnsOnGameThread();
    pumpNativeInvasionRewardsOnGameThread();
    pumpNativeInvasionNoticesOnGameThread();
    pumpNativeInvasionNetworkOnGameThread();
    pumpNativeInvasionHudOnGameThread();
    pumpReliableNpcKillsOnGameThread();
    pumpNativeItemUseBindingsOnGameThread();
    static uint64_t lastSaveWorldSession = 0;
    if (!invasionWorldReady) {
        lastSaveWorldSession = 0;
    } else if (lastSaveWorldSession != invasionWorldEpoch) {
        updateNativeInvasionWorldPath(true, invasionWorldEpoch);
        lastSaveWorldSession = invasionWorldEpoch;
    }
    universalNativeService.store(pumpNativeAccessOnGameThread,std::memory_order_release);
    pumpNativeAccessOnGameThread();
    managedSound::pump();
    localization::pumpOnGameThread();
    gore::pumpOnGameThread();
    pumpUniversalHookInstallsOnGameThread();
    touch::pumpNativeTouchObserverOnGameThread();
    touch::pumpNativeTouchEditorLabelsOnGameThread();
    touch::ensureNativeTouchStaticMappingsOnGameThread();
    touch::pumpNativeTouchIconsOnGameThread();
    touch::pumpNativeTouchIconFrameHooksOnGameThread();
    touch::pumpNativeTouchQuickDrawHookOnGameThread();
    touch::pumpNativeTouchPointerHookOnGameThread();
    touch::pumpNativeTouchLayoutHooksOnGameThread();
    touch::pumpNativeTouchVisibilityHookOnGameThread();
    touch::pumpNativeTouchActivationOnGameThread();
    if (g_worldSessionReady.load(std::memory_order_acquire) &&
        touch::hostReady()) {
        static uint64_t touchTick = 0;
        queueManagedTouchTriggerTick(++touchTick);
    }
    if (nativeWorldNetActive()) {
        const uint64_t currentEpoch = g_worldEpoch.load(std::memory_order_acquire);
        worldNetQueue().pump(currentEpoch,
            [currentEpoch](const worldsave::WorldNetAsync::Result& result) {
                if (!result.success) {
                    BL_WARN("CSharp WorldNet async %s failed: %s",
                        result.job.kind == worldsave::WorldNetAsync::Kind::Send
                            ? "send" : "receive", result.error.c_str());
                    return;
                }
                if (result.job.kind != worldsave::WorldNetAsync::Kind::Send ||
                    g_worldDataEnteredEpoch.load(std::memory_order_acquire) != currentEpoch ||
                    readActivationContext().netMode != 2) return;
                std::string error;
                if (!worldsave::sendNativeWorldNetPacket(
                    result.job.packet, result.job.remote, result.job.ignore, &error))
                    BL_WARN("CSharp WorldNet native send failed: %s", error.c_str());
            });
    }
    if (managedEntitySourceModsRegistered() &&
        runtime::modNpcsSettled() && runtime::modProjectilesSettled()) {
        installNpcAiHost();
        installManagedEntityProjectileHook();
        installManagedSourceEntityHitHook();
        installManagedSourceEntityKillHook();
    }
    pumpInteractiveConsoleOnGameThread();
    pumpBasicRecipesOnGameThread();
    angler::pumpOnGameThread();
    pumpNativeVisualRendererOnGameThread();
    if (!g_worldTilesLoaded.load(std::memory_order_acquire))
        updateManagedNpcBuffDispatcherOnGameThread();
    if (!g_worldTilesLoaded.load(std::memory_order_acquire) &&
        g_initialCSharpRegistrationFinished.load(std::memory_order_acquire) &&
        managedPrefixGameplayReady()) {
        static unsigned restorationTick = 0;
        if (++restorationTick % 120u == 0 &&
            bl::runtime::pendingManagedPrefixCountForActivePlayer() > 0)
            bl::runtime::retryPendingManagedPrefixesOnWorldReady();
    }
    flushDeferredWorldEntry();
    // Saving and quitting changes the menu before it writes the world. Keep
    // the epoch/store alive until the original native quit callback returns.
    if (g_worldQuitSaveFence.active()) return;
    if (!g_worldTilesLoaded.load(std::memory_order_acquire)) {
        if (g_worldDataEnteredEpoch.load(std::memory_order_acquire) != 0 &&
            g_worldMenuPollFrames.fetch_add(1, std::memory_order_relaxed) % 30 == 0) {
            const auto context = readActivationContext();
            if (context.gameMenu == 1 && context.netMode == 0) {
                std::lock_guard<std::mutex> sessionGuard(g_worldTransitionMutex);
                if (g_worldQuitSaveFence.active()) return;
                const uint64_t leaving = g_worldDataEnteredEpoch.exchange(
                    0, std::memory_order_acq_rel);
                if (leaving != 0) {
                    std::string error;
                    worldsave::leaveManagedWorld(leaving, &error);
                    g_worldEpoch.fetch_add(1, std::memory_order_acq_rel);
                    invalidateManagedWorldMonoRequests();
                }
            }
        }
        static unsigned retries = 0;
        if (++retries % 12u == 0) schedulePreWorldBindings();
        reportCSharpLoadSummary();
        return;
    }
    if (g_worldSessionReady.load(std::memory_order_acquire)) {
        static uint64_t lastInvasionWorld = 0;
        static unsigned invasionFrames = 0;
        const uint64_t worldSession = g_worldEpoch.load(std::memory_order_acquire);
        if (lastInvasionWorld != worldSession) {
            lastInvasionWorld = worldSession;
            invasionFrames = 0;
        }
        if (++invasionFrames % 30 == 0 && managedEntitySourceModsRegistered()) {
            const TerrariaSnapshot snapshot = captureTerrariaSnapshot(false, false);
            const int mode = bl::runtime::netMode();
            if (invasionFrames == 30)
                BL_INFO("[CSharp][ItemUse] Stage=WorldTickSnapshot Ready=%d Player=%d Life=%d Mode=%d Result=%s",
                    snapshot.ready ? 1 : 0, snapshot.player.index, snapshot.player.life,
                    mode, snapshot.ready && snapshot.player.life > 0 ? "Snapshot" : "NativePlayerFallback");
            if (snapshot.ready && snapshot.player.life > 0 &&
                snapshot.player.index >= 0 && snapshot.player.index < 255) {
                scheduleManagedInvasionWorldTicks(worldSession, mode,
                    snapshot.player.index, snapshot.player.x, snapshot.player.y);
            } else if ((mode == 0 || mode == 2) && runtime::resolveGameRefs()) {
                struct Point { float x, y; };
                static FieldInfo* playerArray = nullptr;
                static int activeOffset = -1;
                static int lifeOffset = -1;
                if (!playerArray || activeOffset < 0 || lifeOffset < 0) {
                    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
                    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
                    playerArray = main ? il2cpp::findField(main, "player") : nullptr;
                    activeOffset = player ? il2cpp::fieldOffset(player, "active") : -1;
                    lifeOffset = player ? il2cpp::fieldOffset(player, "statLife") : -1;
                }
                if (playerArray && activeOffset >= 0 && lifeOffset >= 0) {
                    Il2CppArray* players = nullptr;
                    il2cpp::api().field_static_get_value(playerArray, &players);
                    if (players) {
                        const int count = std::min<int>(players->length, 255);
                        for (int i = 0; i < count; ++i) {
                            Il2CppObject* player =
                                static_cast<Il2CppObject**>(arrayData(players))[i];
                            if (!player ||
                                runtime::field<uint8_t>(player, activeOffset) == 0 ||
                                runtime::field<int>(player, lifeOffset) <= 0) continue;
                            const auto pos = runtime::field<Point>(
                                player, runtime::game().entity.position);
                            if (std::isfinite(pos.x) && std::isfinite(pos.y))
                                scheduleManagedInvasionWorldTicks(
                                    worldSession, mode, i, pos.x, pos.y);
                            break;
                        }
                    }
                }
            }
        }
        graphics::pumpCSharpScreenEffects();
        const uint64_t tick = g_visualUpdateTick.fetch_add(1, std::memory_order_relaxed) + 1;
        if (tick % 12 == 0) {
            static const std::string sdk = folderOfCheckpoint() + "BunnySourceSdk.dll";
            pollManagedVisualCommands(sdk, 8);
        }
        advanceManagedVisualState(tick, 32);
    }
    if (g_worldSessionReady.load(std::memory_order_acquire)) {
        reportCSharpLoadSummary();
        if (g_worldMenuPollFrames.fetch_add(
                1, std::memory_order_relaxed) % 30 != 0) return;
        const ActivationContext context = readActivationContext();
        if (context.gameMenu != 1 &&
            context.netMode >= 0 && context.netMode <= 3) {
            if (worldIsAuthoritative(context.netMode)) {
                retryWorldManagedPrefixesWhenReady();
                showWorldPlayerMessagesWhenReady();
                updateManagedNpcBuffDispatcherOnGameThread();
            }
            return;
        }
        std::lock_guard<std::mutex> sessionGuard(g_worldTransitionMutex);
        if (g_worldQuitSaveFence.active()) return;
        nativeCommandMailbox().newWorld();
        resetManagedCommandSession();
        managedSound::worldExit();
        clearManagedVisualCommands();
        gore::worldBoundary();
        graphics::clearCSharpScreenEffects();
        pauseManagedScreenSceneCallbacks();
        resetNativeVisualRenderer();
        g_visualUpdateTick.store(0, std::memory_order_relaxed);
        touch::registry().resetWorld();
        touch::resetNativeTouchGroupSession();
        touch::invalidateNativeTouchLayouts();
        g_worldExits.fetch_add(1, std::memory_order_relaxed);
        resetNativeTileSessionState();
        angler::invalidateWorldSession();
        const uint64_t departingEpoch = g_worldEpoch.fetch_add(
            1, std::memory_order_acq_rel);
        const uint64_t loadedEpoch = g_worldDataEnteredEpoch.exchange(
            0, std::memory_order_acq_rel);
        if (loadedEpoch == departingEpoch) {
            std::string unloadError;
            if (!worldsave::leaveManagedWorld(departingEpoch, &unloadError))
                BL_WARN("CSharp WorldSave unload: %s", unloadError.c_str());
        }
        worldsave::activeWorldStore().endWorld(departingEpoch);
        invalidateManagedWorldMonoRequests();
        if (g_worldSystemModules.load(std::memory_order_acquire) > 0)
            worldNetQueue().invalidate();
        g_worldSessionReady.store(false, std::memory_order_release);
        setNativeInvasionWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionRewardWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionNoticeWorld(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionNetWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    setNativeInvasionHudWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
    updateNativeInvasionWorldPath(false, g_worldEpoch.load(std::memory_order_acquire));
        setNativeItemUseWorldState(false, g_worldEpoch.load(std::memory_order_acquire));
        g_worldTilesLoaded.store(false, std::memory_order_release);
        g_worldReadyFrames.store(0, std::memory_order_relaxed);
        g_worldPrefixesRestored.store(false, std::memory_order_release);
        g_preWorldBindingsArmed.store(false, std::memory_order_release);
        g_liveBoundEpoch.store(0, std::memory_order_release);
        g_worldPlayerMessagesShown.store(false, std::memory_order_release);
        if (managedLiveHitStatus().enabled)
            pauseManagedLiveHit();
        pauseManagedPlayerHurtBindings();
        disableUniversalHooks();
        resetNativeAccessOnGameThread();
        pauseUniversalHookMods();
        pauseManagedEntitySourceCallbacks();
        runtime::resetManagedDustLifecycle();
        resetManagedDustSourceWorld();
        resetReliableNpcKills();
        resetManagedSourceNpcSlots();
    resetManagedSourceProjectileSlots();
        {
            std::lock_guard<std::mutex> guard(g_lock);
            for (auto& state : g_statuses)
                state.worldReady = false;
        }
        BL_INFO("csharp auto live: callbacks pausados ao sair do mundo");
        return;
    }
    if (!worldPlayerReady()) {
        g_worldReadyFrames.store(0, std::memory_order_relaxed);
        return;
    }
    if (g_worldReadyFrames.fetch_add(1, std::memory_order_relaxed) >= 2) {
        const ActivationContext context = readActivationContext();
        if (context.netMode < 0 || context.netMode > 3 ||
            context.gameMenu == 1) return;
        g_worldSessionReady.store(true, std::memory_order_release);
        requestBasicItemInstanceRefresh("WorldLoaded");
        gore::worldReady();
        enableUniversalHooks();
        resumeUniversalHookMods();
        BL_INFO("csharp auto live: mundo carregado e jogador ativo confirmados");
        refreshManagedPrefixEligibilityOnWorldReady();
        retryWorldManagedPrefixesWhenReady();
        reportCSharpLoadSummary();
        showWorldPlayerMessagesWhenReady();
        scheduleAutoLiveBindings();
    }
}

bool preWorldCSharpContentReady() {
    if (gore::pendingInstall()) return false;
    bool hasCSharpMods = false;
    for (const auto& source : sourceModStatuses())
        if (source.loaded && source.sourceFiles > 0) {
            hasCSharpMods = true;
            break;
        }
    if (!hasCSharpMods) return true;
    if (!g_initialCSharpRegistrationFinished.load(std::memory_order_acquire) ||
        !g_preWorldBindingsArmed.load(std::memory_order_acquire))
        return false;
    const auto content = basicItemLoadStatus();
    bool needsNpcDispatcher = false;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        needsNpcDispatcher = !g_pendingLiveSpecs.empty();
    }
    const auto context = readActivationContext();
    const bool networkReady = context.netMode <= 0 ||
        g_nativeWorldNetHooksReady.load(std::memory_order_acquire);
    return g_worldRegistrationReady.load(std::memory_order_acquire) &&
           (g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
            (g_nativeWorldSaveHooksReady.load(std::memory_order_acquire) &&
             networkReady)) &&
           content.pendingBehaviors == 0 && content.pendingRecipes == 0 &&
           content.pendingPlacements == 0 && pendingNativeVisualRenderer() == 0 &&
           (!needsNpcDispatcher || managedNpcBuffEffectStatus().gameHookInstalled);
}

bool finishPreWorldCSharpPlayer() {
    requestBasicItemInstanceRefresh("BeforeWorld");
    if (managedPrefixGameplayReady()) {
        refreshManagedPrefixEligibilityOnWorldReady();
        bl::runtime::retryPendingManagedPrefixesOnWorldReady();
    }
    const int unresolved = bl::runtime::pendingManagedPrefixCountForActivePlayer();
    if (unresolved > 0)
        BL_WARN("[CSharp][WARN][PrefixRestore] Stage=BeforeWorld Pending=%d Result=PreservedInSave Reason=NativePrefixRejected", unresolved);
    g_worldPrefixesRestored.store(true, std::memory_order_release);
    return true;
}

void hkCSharpPlayWorld(const MethodInfo* method) {
    if (preWorldCSharpContentReady() && finishPreWorldCSharpPlayer()) {
        if (g_originalPlayWorld) g_originalPlayWorld(method);
        return;
    }
    if (!g_delayedWorldEntry.exchange(true, std::memory_order_acq_rel)) {
        g_delayedPlayWorldMethod = method;
        g_delayedWorldSince = std::chrono::steady_clock::now();
        BL_INFO("C# preparando mods antes de carregar o mundo...");
    }
}

void flushDeferredWorldEntry() {
    if (!g_delayedWorldEntry.load(std::memory_order_acquire)) return;
    if (preWorldCSharpContentReady() && finishPreWorldCSharpPlayer()) {
        const MethodInfo* method = g_delayedPlayWorldMethod;
        g_delayedWorldEntry.store(false, std::memory_order_release);
        BL_INFO("C# conteudo pronto; continuando entrada no mundo");
        if (g_originalPlayWorld) g_originalPlayWorld(method);
        return;
    }
    const auto now=std::chrono::steady_clock::now();
    const auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(now-g_delayedWorldSince).count();
    const auto content=basicItemLoadStatus();
    const int pendingArmor=pendingNativeArmorPolicies();
    const int pendingEquipment=static_cast<int>(pendingManagedNativeEquipTextures());
    const int pendingCore=std::max(0,content.pendingBehaviors-pendingArmor-pendingEquipment);
    const size_t pendingNativePolicies=pendingNativeItemPolicies();
    const bool deferredNativePolicies=pendingCore>0&&static_cast<size_t>(pendingCore)<=pendingNativePolicies;
    static auto lastPendingReport=std::chrono::steady_clock::time_point{};
    if (now-lastPendingReport>=std::chrono::seconds(3)) {
        lastPendingReport=now;
        const bool initialRegistration=!g_initialCSharpRegistrationFinished.load(std::memory_order_acquire);
        bl::log::write(initialRegistration?ANDROID_LOG_DEBUG:ANDROID_LOG_WARN,
                "[CSharp][WorldLoading] Stage=BeforeWorld ElapsedSeconds=%lld PendingCore=%d PendingNativePolicies=%zu PendingArmorSets=%d PendingEquipmentTextures=%d PendingRecipes=%d PendingPlacements=%d PendingVisuals=%zu MonoBindingsReady=%d ModItemsReady=%d",
                static_cast<long long>(elapsed),pendingCore,pendingNativePolicies,pendingArmor,pendingEquipment,content.pendingRecipes,content.pendingPlacements,
                pendingNativeVisualRenderer(),g_preWorldBindingsArmed.load(std::memory_order_acquire)?1:0,
                bl::runtime::modItemsSettled()?1:0);
        static std::map<int,std::string> reportedNativePolicies;
        if(pendingNativePolicies)for(const auto& policy:nativeItemPolicyStatus())if(!policy.ready){
            const std::string reason=policy.error.empty()?policy.phase:policy.error.substr(0,180);
            if(reportedNativePolicies[policy.type]==reason)continue;
            reportedNativePolicies[policy.type]=reason;
            bl::log::write(initialRegistration&&policy.error.empty()?ANDROID_LOG_DEBUG:ANDROID_LOG_WARN,
                    "[CSharp][WorldLoading] Stage=PendingNativePolicy Type=%d Mod=%s Class=%s Detail=%s",
                    policy.type,policy.mod.c_str(),policy.name.c_str(),reason.c_str());
        }
    }
    const bool worldAssetsOnly = worldDependentAssetsMayDefer(
        pendingCore, content.pendingRecipes, content.pendingPlacements,
        g_initialCSharpRegistrationFinished.load(std::memory_order_acquire),
        g_preWorldBindingsArmed.load(std::memory_order_acquire),bl::runtime::modItemsSettled());
    if (!gore::pendingInstall() && (worldAssetsOnly || (elapsed>=6&&(pendingCore==0||deferredNativePolicies)))&&content.pendingPlacements==0&&
        g_initialCSharpRegistrationFinished.load(std::memory_order_acquire)&&
        g_preWorldBindingsArmed.load(std::memory_order_acquire)&&
        g_worldRegistrationReady.load(std::memory_order_acquire)&&
        (g_worldSystemModules.load(std::memory_order_acquire)==0 ||
         (g_nativeWorldSaveHooksReady.load(std::memory_order_acquire)&&
          (readActivationContext().netMode<=0 ||
           g_nativeWorldNetHooksReady.load(std::memory_order_acquire))))&&
        bl::runtime::modItemsSettled()) {
        BL_WARN("[CSharp][WorldLoading] Stage=BeforeWorld Result=ContinueWithDeferredNativeAssets PendingNativePolicies=%zu PendingArmorSets=%d PendingEquipmentTextures=%d PendingRecipes=%d PendingVisuals=%zu",
                pendingNativePolicies,pendingArmor,pendingEquipment,content.pendingRecipes,pendingNativeVisualRenderer());
        if (finishPreWorldCSharpPlayer()) {
            const MethodInfo* method=g_delayedPlayWorldMethod;
            g_delayedWorldEntry.store(false,std::memory_order_release);
            if (g_originalPlayWorld)g_originalPlayWorld(method);
            return;
        }
    }
    if (elapsed<30)return;
    if (!g_delayedWorldEntry.exchange(false,std::memory_order_acq_rel))return;
    BL_ERROR("[CSharp][ERROR][WorldLoading] Stage=BeforeWorld Reason=ContentNotReady Timeout=30s Result=WorldEntryCancelled "
             "PendingItems=%d PendingRecipes=%d PendingPlacements=%d PendingVisuals=%zu PendingPrefixes=%d MonoBindingsReady=%d",
             content.pendingBehaviors,content.pendingRecipes,content.pendingPlacements,pendingNativeVisualRenderer(),
             bl::runtime::pendingManagedPrefixCountForActivePlayer(),
             g_preWorldBindingsArmed.load(std::memory_order_acquire)?1:0);
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    const MethodInfo* setMenu = main
        ? il2cpp::api().class_get_method_from_name(main, "set_menuMode", 1)
        : nullptr;
    if (setMenu) {
        int mode = 6;
        void* args[1] = {&mode};
        Il2CppObject* exception = nullptr;
        il2cpp::api().runtime_invoke(setMenu, nullptr, args, &exception);
        if (exception)
            BL_ERROR("[CSharp][ERROR][WorldLoading] Stage=ReturnToWorldList Reason=NativeMenuException");
    }
}



using NativeClearWorldFn = void (*)(const MethodInfo*);
NativeClearWorldFn g_origNativeWorldClear = nullptr;

void hkNativeCSharpWorldClear(const MethodInfo* method) {
    if (!g_origNativeWorldClear) return;
    g_origNativeWorldClear(method);
    if (g_worldSystemModules.load(std::memory_order_acquire) > 0)
        worldNetQueue().invalidate();
    if (g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
        !g_worldRegistrationReady.load(std::memory_order_acquire) ||
        !g_nativeWorldNetHooksReady.load(std::memory_order_acquire) ||
        readActivationContext().netMode != 1) return;
    const uint64_t old = g_worldDataEnteredEpoch.exchange(
        0, std::memory_order_acq_rel);
    if (old != 0) {
        std::string error;
        worldsave::leaveManagedWorld(old, &error);
    }
    invalidateManagedWorldMonoRequests();
    const uint64_t epoch = g_worldEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
    worldsave::MonoWorldRequest request{};
    request.action = worldsave::MonoWorldAction::Enter;
    request.epoch = epoch;
    request.authoritative = false;
    worldsave::MonoWorldReply reply{};
    if (!requestManagedWorldOnMono(request, &reply, 3000) || !reply.success) {
        BL_WARN("CSharp WorldNet client world initialization rejected: %s",
                reply.error.c_str());
        return;
    }
    g_worldDataEnteredEpoch.store(epoch, std::memory_order_release);
}

using NativeLoadWorldFn = void (*)(bool, const MethodInfo*);
using NativeSaveWorldFn = void (*)(bool, bool, const MethodInfo*);
using NativeQuitCallbackFn = void (*)(Il2CppObject*, const MethodInfo*);
NativeLoadWorldFn g_origNativeWorldLoad = nullptr;
NativeSaveWorldFn g_origNativeWorldSave = nullptr;
NativeQuitCallbackFn g_origNativeQuitCallback = nullptr;

void hkNativeCSharpWorldLoad(bool check, const MethodInfo* method) {
    if (!g_origNativeWorldLoad) return;
    g_origNativeWorldLoad(check, method);
    if (g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
        !g_worldRegistrationReady.load(std::memory_order_acquire) ||
        !g_worldTilesLoaded.load(std::memory_order_acquire) ||
        (readActivationContext().netMode != 0 &&
         readActivationContext().netMode != 2)) return;
    const uint64_t epoch = g_worldEpoch.load(std::memory_order_acquire);
    if (!worldsave::activeWorldStore().active(epoch)) return;
    std::string error;
    if (!worldsave::enterManagedWorld(epoch, true, &error)) {
        g_worldDataEnteredEpoch.store(0, std::memory_order_release);
        BL_ERROR("CSharp WorldSave load blocked: %s", error.c_str());
        return;
    }
    if (g_worldEpoch.load(std::memory_order_acquire) != epoch) {
        std::string ignored;
        worldsave::leaveManagedWorld(epoch, &ignored);
        return;
    }
    g_worldDataEnteredEpoch.store(epoch, std::memory_order_release);
}

void hkNativeCSharpWorldSave(bool cloud, bool reset, const MethodInfo* method) {
    if (!g_origNativeWorldSave) return;
    // Save do Terraria primeiro, sidecar apenas depois de retornar normalmente.
    g_origNativeWorldSave(cloud, reset, method);
    if (cloud || g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
        (readActivationContext().netMode != 0 &&
         readActivationContext().netMode != 2)) return;
    const uint64_t epoch = g_worldEpoch.load(std::memory_order_acquire);
    if (g_worldDataEnteredEpoch.load(std::memory_order_acquire) != epoch ||
        !worldsave::activeWorldStore().active(epoch)) return;
    std::string error;
    if (!worldsave::saveManagedWorld(epoch, true, &error))
        BL_ERROR("CSharp WorldSave commit not confirmed: %s", error.c_str());
}


void hkNativeCSharpWorldQuitCallback(Il2CppObject* context, const MethodInfo* method) {
    if (!g_origNativeQuitCallback) return;
    worldsave::WorldQuitSaveFence::Scope saving(g_worldQuitSaveFence, g_worldTransitionMutex);
    g_origNativeQuitCallback(context, method);
    const uint64_t epoch = g_worldDataEnteredEpoch.exchange(
        0, std::memory_order_acq_rel);
    if (epoch == 0) return;
    std::string error;
    if (!worldsave::leaveManagedWorld(epoch, &error))
        BL_WARN("CSharp WorldSave quit unload: %s", error.c_str());
    if (g_worldSystemModules.load(std::memory_order_acquire) > 0)
        worldNetQueue().invalidate();
    invalidateManagedWorldMonoRequests();
}

bool installNativeCSharpWorldSaveHooks() {
    Il2CppClass* worldFile = il2cpp::findClass({"Terraria.IO", "WorldFile", {}});
    if (!worldFile) return false;
    auto& api = il2cpp::api();
    const auto loadSig = il2cpp::parseSignature(
        "void LoadWorld(bool canCheckFileState)");
    const auto saveSig = il2cpp::parseSignature(
        "void InternalSaveWorld(bool useCloudSaving, bool resetTime)");
    const auto quitSig = il2cpp::parseSignature(
        "void SaveAndQuitCallBack(object threadContext)");
    if (!loadSig.valid || !saveSig.valid || !quitSig.valid) return false;
    const MethodInfo* load = il2cpp::findMethodBySignature(worldFile, loadSig);
    const MethodInfo* save = il2cpp::findMethodBySignature(worldFile, saveSig);
    Il2CppClass* worldGen = il2cpp::findClass({"Terraria", "WorldGen", {}});
    const MethodInfo* quit = worldGen
        ? il2cpp::findMethodBySignature(worldGen, quitSig) : nullptr;
    const MethodInfo* clear = worldGen
        ? il2cpp::findMethodBySignature(worldGen,
            il2cpp::parseSignature("void clearWorld()")) : nullptr;
    if (!load || !save || !quit || !clear || !api.method_is_instance ||
        api.method_is_instance(load) || api.method_is_instance(save) ||
        api.method_is_instance(quit) || api.method_is_instance(clear)) {
        BL_WARN("CSharp WorldSave: assinatura IL2CPP nao encontrada para hooks estaticos");
        return false;
    }
    if (!bl::hook::install(load, hkNativeCSharpWorldLoad, &g_origNativeWorldLoad))
        return false;
    if (!bl::hook::install(save, hkNativeCSharpWorldSave, &g_origNativeWorldSave))
        return false;
    if (!bl::hook::install(quit, hkNativeCSharpWorldQuitCallback,
                           &g_origNativeQuitCallback)) return false;
    if (!bl::hook::install(clear, hkNativeCSharpWorldClear,
                           &g_origNativeWorldClear)) return false;
    return true;
}

bool installCSharpWorldEntryGate() {
    Il2CppClass* worldGen = il2cpp::findClass({"Terraria", "WorldGen", {}});
    const MethodInfo* playWorld = worldGen
        ? il2cpp::api().class_get_method_from_name(worldGen, "playWorld", 0)
        : nullptr;
    if (!playWorld || !bl::hook::install(playWorld, hkCSharpPlayWorld, &g_originalPlayWorld)) {
        BL_ERROR("[CSharp][ERROR][WorldLoading] Stage=InstallEntryHook Reason=WorldGenPlayWorldUnavailable");
        return false;
    }
    const bool worldSaveReady = installNativeCSharpWorldSaveHooks();
    const bool worldNetReady = worldsave::installNativeWorldNetHooks();
    g_nativeWorldNetHooksReady.store(worldNetReady, std::memory_order_release);
    g_nativeWorldSaveHooksReady.store(worldSaveReady, std::memory_order_release);
    if (!worldSaveReady)
        BL_WARN("CSharp WorldSave hooks unavailable; world system gate remains closed");
    return true;
}


bool nativeWorldNetActive() {
    return g_worldSystemModules.load(std::memory_order_acquire) > 0 &&
        g_nativeWorldNetHooksReady.load(std::memory_order_acquire);
}


worldsave::WorldNetAsync& worldNetQueue() {
    static worldsave::WorldNetAsync queue(
        [](const worldsave::WorldNetAsync::Job& job,
           std::vector<uint8_t>* packet, std::string* error) -> bool {
            if (job.kind == worldsave::WorldNetAsync::Kind::Receive)
                return worldsave::receiveWorldNetworkPacket(
                    job.epoch, packet->data(), packet->size(), error);
            return worldsave::prepareWorldNetworkPacket(job.epoch, packet, error);
        });
    return queue;
}

void onNativeWorldSyncSent(int remoteClient, int ignoreClient) {
    if (g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
        !g_nativeWorldNetHooksReady.load(std::memory_order_acquire) ||
        readActivationContext().netMode != 2) return;
    const uint64_t epoch = g_worldEpoch.load(std::memory_order_acquire);
    if (g_worldDataEnteredEpoch.load(std::memory_order_acquire) != epoch)
        return;
    if (!worldNetQueue().send(epoch, remoteClient, ignoreClient))
        BL_WARN("CSharp WorldNet send queue full or invalid");
}

void onNativeWorldNetReceived(const uint8_t* bytes, size_t length, int from) {
    if (g_worldSystemModules.load(std::memory_order_acquire) == 0 ||
        !g_nativeWorldNetHooksReady.load(std::memory_order_acquire) ||
        readActivationContext().netMode != 1) return;
    const uint64_t epoch = g_worldDataEnteredEpoch.load(std::memory_order_acquire);
    if (epoch == 0 || epoch != g_worldEpoch.load(std::memory_order_acquire))
        return;
    if (from != 256 || !worldsave::validWorldNetworkPacket(bytes, length) ||
        !worldNetQueue().receive(epoch, bytes, length))
        BL_WARN("CSharp WorldNet received invalid or full queue (remote=%d)", from);
}

void startAutoSourceRuntime() {
    if (g_started.exchange(true, std::memory_order_acq_rel)) return;
    g_sourceStart = std::chrono::steady_clock::now();
    std::thread(run).detach();
}

std::vector<AutoSourceStatus> autoSourceStatuses() {
    std::lock_guard<std::mutex> lock(g_lock);
    return g_statuses;
}

AutoWorldLifecycleStatus autoWorldLifecycleStatus() {
    AutoWorldLifecycleStatus state;
    state.epoch = g_worldEpoch.load(std::memory_order_acquire);
    state.worldLoads = g_worldLoads.load(std::memory_order_relaxed);
    state.worldExits = g_worldExits.load(std::memory_order_relaxed);
    state.rebindSchedules =
        g_worldRebindSchedules.load(std::memory_order_relaxed);
    state.worldReady = g_worldSessionReady.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(g_lock);
        state.configuredBindings = static_cast<int>(g_pendingLiveSpecs.size());
    }
    return state;
}

}


