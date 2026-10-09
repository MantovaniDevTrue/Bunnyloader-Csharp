#include "experiments/csharp/AutoSourceRuntime.h"

#include "content/projectiles/ModProjectiles.h"
#include "core/Config.h"
#include "core/Log.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "experiments/csharp/SourceLoader.h"
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
constexpr const char* kCacheVersion = "BUNNY_AUTO_SOURCE_V1_0_1_10_16_48";
std::mutex g_lock;
std::vector<AutoSourceStatus> g_statuses;
std::atomic<bool> g_started{false};

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
                 uint64_t* hash, int* count, int depth) {
    if (depth > 7 || *count > 32) return false;
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
            if (!hashSources(dir, rel, hash, count, depth + 1)) return false;
        } else if (S_ISREG(metadata.st_mode) &&
                   name.size() > 3 && name.substr(name.size() - 3) == ".cs") {
            if (metadata.st_size < 1 || metadata.st_size > 256 * 1024 || ++*count > 32)
                return false;
            std::string source = readText(full, 256 * 1024);
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
    if (!hashSources(root + "/content", "", &hash, &count, 0) || count < 1)
        return {};
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

bool readCache(const std::string& path, const std::string& id,
               const std::string& key, std::string* defaults,
               std::string* curves, std::string* combat) {
    const std::string value = readText(path, 65536);
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
        defaultsSize < 10 || defaultsSize > 8192 ||
        curvesSize > 22000 || combatSize < 25 || combatSize > 30000)
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
    if (defaults.empty() || defaults.size() > 8192 || curves.size() > 22000 ||
        combat.size() < 25 || combat.size() > 30000) return;
    const std::string value = std::string(kCacheVersion) + "\n" + id + "\n" +
        key + "\n" + std::to_string(defaults.size()) + " " +
        std::to_string(curves.size()) + " " + std::to_string(combat.size()) +
        "\n" + defaults + curves + combat;
    writeText(path, value);
}

bool validCompileStatus(const std::string& status, const std::string& id,
                        const SourceModStatus& mod) {
    return status.find("state=finished\n") != std::string::npos &&
           status.find("modId=" + id + "\n") != std::string::npos &&
           status.find("phase=compile-verified\n") != std::string::npos &&
           status.find("assemblyReady=1\n") != std::string::npos &&
           status.find("executedTypes=" + std::to_string(mod.items + mod.projectiles + mod.buffs + mod.players + mod.npcs) +
                       "\n") != std::string::npos;
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
    for (int poll = 0; poll < 600; ++poll) {
        std::this_thread::sleep_for(300ms);
        const std::string status = readText(path, 8192);
        if (status.find("state=failed\n") != std::string::npos &&
            (status.find("modId=" + id + "\n") != std::string::npos ||
             status.find("modId=") == std::string::npos)) {
            *error = "Roslyn/MonoVM falhou; verifique sourceCompileStatus";
            return false;
        }
        if (!validCompileStatus(status, id, mod)) continue;
        *defaults = readText(workDir + "source-runtime-values.txt", 8192);
        *curves = readText(workDir + "source-projectile-turns.txt", 22000);
        *combat = readText(workDir + "source-combat-policies.txt", 30000);
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
        if (++count > 16 || line.size() > 1300) return false;
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
        if (++total > 16 || line.size() > 3200) return false;
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
};

ActivationContext readActivationContext() {
    ActivationContext result;
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    FieldInfo* netMode = main ? il2cpp::findField(main, "netMode") : nullptr;
    FieldInfo* menu = main ? il2cpp::findField(main, "gameMenu") : nullptr;
    if (netMode)
        il2cpp::api().field_static_get_value(netMode, &result.netMode);
    if (menu) {
        uint8_t inMenu = 1;
        il2cpp::api().field_static_get_value(menu, &inMenu);
        result.gameMenu = inMenu ? 1 : 0;
    }
    return result;
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
        note(&status, "waiting-content");
        bool ready = false;
        for (int i = 0; i < 300; ++i) {
            if (runtime::modProjectilesSettled()) {
                ready = true;
                break;
            }
            std::this_thread::sleep_for(300ms);
        }
        if (!ready) {
            note(&status, "failed", "Tabelas de projetil nao ficaram prontas");
            continue;
        }
        const std::string root = mods::rootOf(mod.id);
        const std::string key = fingerprint(mod.id, root);
        if (root.empty() || key.empty()) {
            note(&status, "failed", "Arquivos C# nao encontrados");
            continue;
        }
        std::string defaults, curves, combat, error;
        const std::string cacheFile = cacheDir + "/" + mod.id + "-" + key + ".cache";
        status.cached = readCache(cacheFile, mod.id, key, &defaults, &curves, &combat);
        if (!status.cached) {
            note(&status, "compiling");
            if (!compile(root, mod.id, mod, work, &defaults, &curves, &combat, &error)) {
                note(&status, "failed", error);
                continue;
            }
        }
        note(&status, "applying");
        if (!applyManagedSourceDefaults(mod.id, defaults, &error)) {
            note(&status, "failed", error);
            continue;
        }
        status.applied = true;
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
        if (!status.cached) saveCache(cacheFile, mod.id, key, defaults, curves, combat);
        note(&status, entries.empty() && effects.empty() ? "ready" : "waiting-world");
    }
    if (pending.empty() && pendingCombat.empty()) return;
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
}

}

void startAutoSourceRuntime() {
    if (g_started.exchange(true, std::memory_order_acq_rel)) return;
    std::thread(run).detach();
}

std::vector<AutoSourceStatus> autoSourceStatuses() {
    std::lock_guard<std::mutex> lock(g_lock);
    return g_statuses;
}

}
