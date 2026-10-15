#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/AutoSourceRuntime.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "content/items/ModItemSave.h"
#include "hook/HookManager.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Config.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/NpcAiHost.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/ManagedSourceClassCount.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "mods/ModLoader.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
namespace {

constexpr const char* kFixtureId = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
constexpr const char* kCacheVersion = "BUNNY_AUTO_SOURCE_V1_0_1_10_16_90_29_7_preworld_gate";
std::chrono::steady_clock::time_point g_sourceStart{};
std::atomic<int> g_cachedMods{0};
std::atomic<int> g_compiledMods{0};
std::atomic<bool> g_runtimeWarmStarted{false};
std::atomic<bool> g_runtimeWarming{false};
std::mutex g_lock;
std::mutex g_worldTransitionMutex;
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
std::atomic<int> g_worldReadyFrames{0};
std::atomic<bool> g_worldPrefixesRestored{false};
std::atomic<bool> g_worldPlayerMessagesShown{false};
std::atomic<int> g_worldMenuPollFrames{0};
std::atomic<uint64_t> g_worldEpoch{0};
std::atomic<uint64_t> g_visualUpdateTick{0};
std::atomic<uint64_t> g_liveBoundEpoch{0};
std::atomic<int> g_worldLoads{0};
std::atomic<int> g_worldExits{0};
std::atomic<int> g_worldRebindSchedules{0};
std::vector<std::pair<std::string, int>> g_pendingLiveSpecs;
std::string g_pendingLiveWork;

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
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::string result(limit + 1, '\0');
    file.read(result.data(), static_cast<std::streamsize>(result.size()));
    const size_t count = static_cast<size_t>(file.gcount());
    if (count > limit) return {};
    result.resize(count);
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

void hashByte(uint64_t* hash, unsigned char byte) {
    *hash ^= byte;
    *hash *= 1099511628211ULL;
}

void hashString(uint64_t* hash, const std::string& value) {
    for (unsigned char ch : value) hashByte(hash, ch);
    hashByte(hash, 0);
}

bool hashSources(const std::string& dir, const std::string& relative,
                 uint64_t* hash, int* count, size_t* totalBytes, int depth) {
    if (depth > 7 || static_cast<size_t>(*count) > kSourceMaxFiles) return false;
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
            if (!hashSources(dir, rel, hash, count, totalBytes, depth + 1)) return false;
        } else if (S_ISREG(metadata.st_mode) &&
                   name.size() > 3 && name.substr(name.size() - 3) == ".cs") {
            if (metadata.st_size < 1 || metadata.st_size > 256 * 1024 || static_cast<size_t>(++*count) > kSourceMaxFiles)
                return false;
            *totalBytes+=static_cast<size_t>(metadata.st_size);
            if(*totalBytes>kSourceMaxTotalBytes)return false;
            std::string source = readText(full, kSourceMaxFileBytes);
            if (source.empty()) return false;
            hashString(hash, rel);
            hashString(hash, source);
        }
    }
    return true;
}

std::string fingerprint(const std::string& id, const std::string& root) {
    uint64_t hash = 14695981039346656037ULL;
    hashString(&hash, kCacheVersion);
    hashString(&hash, id);
    int count = 0;
    size_t totalBytes=0;
    if (!hashSources(root + "/content", "", &hash, &count, &totalBytes, 0) || count < 1)
        return {};
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

bool readCache(const std::string& path, const std::string& id,
               const std::string& key, std::string* defaults,
               std::string* curves, std::string* combat) {
    const std::string value = readText(path, kSourceDefaultsBytes+kSourceMotionBytes+1024*1024+1024);
    if (value.empty()) return false;
    std::istringstream input(value);
    std::string first, second, third, counts;
    if (!std::getline(input, first) || first != kCacheVersion ||
        !std::getline(input, second) || second != id ||
        !std::getline(input, third) || third != key ||
        !std::getline(input, counts)) return false;
    std::istringstream lengths(counts);
    size_t defaultsSize = 0, curvesSize = 0, combatSize = 0;
    std::string trailing;
    if (!(lengths >> defaultsSize >> curvesSize >> combatSize) ||
        (lengths >> trailing) ||
        defaultsSize < 10 || defaultsSize > kSourceDefaultsBytes ||
        curvesSize > kSourceMotionBytes || combatSize < 25 || combatSize > kSourceMotionBytes)
        return false;
    const size_t header = value.find('\n',
        value.find('\n', value.find('\n', value.find('\n') + 1) + 1) + 1);
    if (header == std::string::npos ||
        value.size() != header + 1 + defaultsSize + curvesSize + combatSize)
        return false;
    const size_t data = header + 1;
    *defaults = value.substr(data, defaultsSize);
    *curves = value.substr(data + defaultsSize, curvesSize);
    *combat = value.substr(data + defaultsSize + curvesSize, combatSize);
    return defaults->rfind("BUNNY_MANAGED_DEFAULTS_V2\n", 0) == 0 &&
           (curves->empty() || curves->rfind("BUNNY_PROJECTILE_TURNS_V1\n", 0) == 0) &&
           combat->rfind("BUNNY_COMBAT_POLICIES_V1\n", 0) == 0;
}

void saveCache(const std::string& path, const std::string& id,
               const std::string& key, const std::string& defaults,
               const std::string& curves, const std::string& combat) {
    if (defaults.empty() || defaults.size() > kSourceDefaultsBytes || curves.size() > kSourceMotionBytes ||
        combat.size() < 25 || combat.size() > kSourceMotionBytes) return;
    const std::string value = std::string(kCacheVersion) + "\n" + id + "\n" +
        key + "\n" + std::to_string(defaults.size()) + " " +
        std::to_string(curves.size()) + " " + std::to_string(combat.size()) +
        "\n" + defaults + curves + combat;
    writeText(path, value);
}

bool validCompileStatus(const std::string& status, const std::string& id,
                        const SourceModStatus& mod) {
    return managedSourceCompileFinished(status, id, mod);
}

bool compile(const std::string& root, const std::string& id,
             const SourceModStatus& mod, const std::string& workDir,
             std::string* defaults, std::string* curves,
             std::string* combat, std::string* error) {
    std::remove((workDir + "source-compile-status.txt").c_str());
    std::remove((workDir + "source-runtime-values.txt").c_str());
    std::remove((workDir + "source-projectile-turns.txt").c_str());
    std::remove((workDir + "source-combat-policies.txt").c_str());
    if (!requestIsolatedMonoCompile(root, id, *error)) return false;
    const std::string path = workDir + "source-compile-status.txt";
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::seconds(180);
    for (; std::chrono::steady_clock::now() < deadline;) {
        const std::string status = readText(path, 8192);
        if (status.find("state=failed\n") != std::string::npos &&
            (status.find("modId=" + id + "\n") != std::string::npos ||
             status.find("modId=") == std::string::npos)) {
            *error = "Roslyn/MonoVM falhou; verifique sourceCompileStatus";
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
    if (mode != 0) return false;
    Il2CppObject* world = nullptr;
    api.field_static_get_value(refs.world, &world);
    if (!world) return false;
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

void run() {
    auto& api = il2cpp::api();
    api.thread_attach(api.domain_get());
    const std::string work = folderOfCheckpoint();
    if (work.empty()) return;
    const std::string cacheDir = work + "auto-source-cache";
    mkdir(cacheDir.c_str(), 0700);
    std::vector<Curve> pending;
    std::vector<Combat> pendingCombat;
    for (const auto& mod : sourceModStatuses()) {
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
        const auto scanStarted = std::chrono::steady_clock::now();
        const std::string root = mods::rootOf(mod.id);
        if (root.empty()) {
            note(&status, "failed", "Arquivos C# nao encontrados");
            continue;
        }
        const std::string key = fingerprint(mod.id, root);
        if (key.empty()) {
            note(&status, "failed", "Arquivos C# nao encontrados");
            continue;
        }
        const auto fingerprintMs = elapsedMilliseconds(scanStarted);
        std::string defaults, curves, combat, error;
        const std::string cacheFile = cacheDir + "/" + mod.id + "-" + key + ".cache";
        const auto readStarted = std::chrono::steady_clock::now();
        status.cached = readCache(cacheFile, mod.id, key, &defaults, &curves, &combat);
        const auto cacheMs = fingerprintMs + elapsedMilliseconds(readStarted);
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
        if (mod.projectiles > 0 &&
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
        if (!status.cached) saveCache(cacheFile, mod.id, key, defaults, curves, combat);
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
                note(&status,"applying");
                defaultsApplied=applyManagedSourceDefaults(mod.id,defaults,&error);
            }
        }
        if (!defaultsApplied) {
            note(&status, "failed", error);
            continue;
        }
        BL_INFO("[CSharp][Startup] Mod=%s Stage=DefaultsRequest Cache=%s WaitTablesMs=%lld CacheReadMs=%lld CompileMs=%lld ApplyAndRecoveryMs=%lld",
                mod.id.c_str(),status.cached?"Hit":"Miss",tablesMs,cacheMs,compileMs,
                elapsedMilliseconds(applyStarted));
        status.applied = true;
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
        status.curves = static_cast<int>(entries.size());
        for (const auto& effect : effects) {
            if (effect.kill) ++status.killPolicies;
            else ++status.hitPolicies;
        }
        pending.insert(pending.end(), entries.begin(), entries.end());
        pendingCombat.insert(pendingCombat.end(), effects.begin(), effects.end());
        note(&status, entries.empty() && effects.empty() ? "ready" : "waiting-world");
    }
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
    const int pending = basic.pendingBehaviors + basic.pendingRecipes +
        basic.pendingPlacements + pendingCallbacks;
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

void noteAutoWorldTilesLoaded() {
    std::lock_guard<std::mutex> sessionGuard(g_worldTransitionMutex);
    clearManagedVisualCommands();
    g_visualUpdateTick.store(0, std::memory_order_relaxed);
    g_worldLoads.fetch_add(1, std::memory_order_relaxed);
    g_worldEpoch.fetch_add(1, std::memory_order_acq_rel);
    g_worldSessionReady.store(false, std::memory_order_release);
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

void noteAutoGameUpdate() {
    pumpBasicRecipesOnGameThread();
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
    if (!g_worldTilesLoaded.load(std::memory_order_acquire)) {
        static unsigned retries = 0;
        if (++retries % 12u == 0) schedulePreWorldBindings();
        reportCSharpLoadSummary();
        return;
    }
    if (g_worldSessionReady.load(std::memory_order_acquire)) {
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
        if (context.gameMenu != 1 && context.netMode == 0) {
            retryWorldManagedPrefixesWhenReady();
            showWorldPlayerMessagesWhenReady();
            updateManagedNpcBuffDispatcherOnGameThread();
            return;
        }
        std::lock_guard<std::mutex> sessionGuard(g_worldTransitionMutex);
        clearManagedVisualCommands();
        g_visualUpdateTick.store(0, std::memory_order_relaxed);
        g_worldExits.fetch_add(1, std::memory_order_relaxed);
        g_worldEpoch.fetch_add(1, std::memory_order_acq_rel);
        g_worldSessionReady.store(false, std::memory_order_release);
        g_worldTilesLoaded.store(false, std::memory_order_release);
        g_worldReadyFrames.store(0, std::memory_order_relaxed);
        g_worldPrefixesRestored.store(false, std::memory_order_release);
        g_preWorldBindingsArmed.store(false, std::memory_order_release);
        g_liveBoundEpoch.store(0, std::memory_order_release);
        g_worldPlayerMessagesShown.store(false, std::memory_order_release);
        if (managedLiveHitStatus().enabled)
            pauseManagedLiveHit();
        pauseManagedPlayerHurtBindings();
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
        if (context.netMode != 0 || context.gameMenu == 1) return;
        g_worldSessionReady.store(true, std::memory_order_release);
        BL_INFO("csharp auto live: mundo carregado e jogador ativo confirmados");
        refreshManagedPrefixEligibilityOnWorldReady();
        retryWorldManagedPrefixesWhenReady();
        reportCSharpLoadSummary();
        showWorldPlayerMessagesWhenReady();
        scheduleAutoLiveBindings();
    }
}

bool preWorldCSharpContentReady() {
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
    return content.pendingBehaviors == 0 && content.pendingRecipes == 0 &&
           content.pendingPlacements == 0 &&
           (!needsNpcDispatcher || managedNpcBuffEffectStatus().gameHookInstalled);
}

bool finishPreWorldCSharpPlayer() {
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
    if (std::chrono::steady_clock::now() - g_delayedWorldSince < std::chrono::minutes(4))
        return;
    if (!g_delayedWorldEntry.exchange(false, std::memory_order_acq_rel))
        return;
    const auto content = basicItemLoadStatus();
    BL_ERROR("[CSharp][ERROR][WorldLoading] Stage=BeforeWorld Reason=ContentNotReady Timeout=240s Result=WorldEntryCancelled "
             "PendingItems=%d PendingRecipes=%d PendingPlacements=%d PendingPrefixes=%d MonoBindingsReady=%d",
             content.pendingBehaviors, content.pendingRecipes, content.pendingPlacements,
             bl::runtime::pendingManagedPrefixCountForActivePlayer(),
             g_preWorldBindingsArmed.load(std::memory_order_acquire) ? 1 : 0);
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

bool installCSharpWorldEntryGate() {
    Il2CppClass* worldGen = il2cpp::findClass({"Terraria", "WorldGen", {}});
    const MethodInfo* playWorld = worldGen
        ? il2cpp::api().class_get_method_from_name(worldGen, "playWorld", 0)
        : nullptr;
    if (!playWorld || !bl::hook::install(playWorld, hkCSharpPlayWorld, &g_originalPlayWorld)) {
        BL_ERROR("[CSharp][ERROR][WorldLoading] Stage=InstallEntryHook Reason=WorldGenPlayWorldUnavailable");
        return false;
    }
    return true;
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


