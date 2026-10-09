#include "experiments/csharp/AutoSourceBootstrap.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Config.h"
#include "core/Log.h"
#include "mods/ModLoader.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace bl::csharp {
namespace {

constexpr const char* kCacheVersion = "BUNNY_CSHARP_AUTOMATIC_V1_0.1.10.16.36";
constexpr const char* kFixture = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";

struct CompiledPolicy {
    std::string name;
    std::array<int, 120> turns{};
};

bool readFile(const std::string& path, std::string* out, size_t limit = 16384) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return false;
    const auto size = input.tellg();
    if (size <= 0 || static_cast<uint64_t>(size) > limit) return false;
    out->resize(static_cast<size_t>(size));
    input.seekg(0);
    return static_cast<bool>(input.read(out->data(), size));
}

bool writeFile(const std::string& path, const std::string& value) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output || !output.write(value.data(), value.size())) return false;
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

bool collectSources(const std::string& base, const std::string& relative,
                    std::vector<std::string>* paths, int depth) {
    if (depth > 7 || paths->size() > 32) return false;
    const std::string dir = base + (relative.empty() ? "" : "/" + relative);
    DIR* opened = opendir(dir.c_str());
    if (!opened) return false;
    std::vector<std::string> names;
    while (dirent* entry = readdir(opened)) {
        const std::string name = entry->d_name;
        if (name != "." && name != ".." && !name.empty() && name[0] != '.')
            names.push_back(name);
    }
    closedir(opened);
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        const std::string path = relative.empty() ? name : relative + "/" + name;
        struct stat st{};
        if (lstat((base + "/" + path).c_str(), &st) != 0 || S_ISLNK(st.st_mode))
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (!collectSources(base, path, paths, depth + 1)) return false;
        } else if (S_ISREG(st.st_mode) && name.size() >= 3 &&
                   name.compare(name.size() - 3, 3, ".cs") == 0) {
            if (st.st_size <= 0 || st.st_size > 256 * 1024 ||
                paths->size() >= 32) return false;
            paths->push_back(path);
        }
    }
    return true;
}

void hashBytes(uint64_t* value, const std::string& bytes) {
    for (unsigned char c : bytes) {
        *value ^= c;
        *value *= 1099511628211ull;
    }
}

std::string fingerprint(const std::string& modId, const std::string& root) {
    std::vector<std::string> sourceFiles;
    if (!collectSources(root + "/content", "", &sourceFiles, 0) ||
        sourceFiles.empty()) return {};
    std::sort(sourceFiles.begin(), sourceFiles.end());
    uint64_t hash = 14695981039346656037ull;
    hashBytes(&hash, kCacheVersion);
    hashBytes(&hash, modId);
    std::string text;
    if (!readFile(root + "/manifest.json", &text, 32768)) return {};
    hashBytes(&hash, text);
    for (const auto& name : sourceFiles) {
        if (!readFile(root + "/content/" + name, &text, 256 * 1024)) return {};
        hashBytes(&hash, name);
        hashBytes(&hash, text);
    }
    char hex[24]{};
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(hash));
    return hex;
}

std::string checkpointDir() {
    const std::string& checkpoint = config().managedCheckpointPath;
    const auto slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos) return {};
    return checkpoint.substr(0, slash);
}

bool safeModId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (unsigned char c : id) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '-' || c == '_'))
            return false;
    }
    return true;
}

bool validResult(const std::string& state, const SourceModStatus& mod) {
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("modId=" + mod.id + "\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        state.find("assemblyReady=1\n") == std::string::npos) return false;
    const auto marker = state.find("executedTypes=");
    if (marker == std::string::npos) return false;
    const char* start = state.c_str() + marker + 14;
    char* end = nullptr;
    errno = 0;
    const long count = std::strtol(start, &end, 10);
    return errno == 0 && end != start && (*end == '\n' || *end == '\r') &&
           count == mod.items + mod.projectiles;
}

bool waitForCompile(const std::string& dir, const SourceModStatus& mod) {
    for (int i = 0; i < 900; ++i) {
        std::string state;
        if (readFile(dir + "/source-compile-status.txt", &state)) {
            if (state.find("state=finished\n") != std::string::npos)
                return validResult(state, mod);
            if (state.find("state=failed\n") != std::string::npos) return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

bool parseCurves(const std::string& data, const SourceModStatus& mod,
                 std::vector<CompiledPolicy>* curves) {
    if (data.empty()) return false;
    std::istringstream lines(data);
    std::string row;
    if (!std::getline(lines, row) || row != "BUNNY_PROJECTILE_TURNS_V1")
        return false;
    while (std::getline(lines, row)) {
        if (row.empty()) continue;
        if (row.size() > 1300 || curves->size() >= 8) return false;
        std::istringstream fields(row);
        CompiledPolicy curve;
        if (!std::getline(fields, curve.name, '\t') ||
            !mod.registeredTypes.contains(curve.name) ||
            runtime::modProjectileTypeByName(mod.id, curve.name) < 0)
            return false;
        for (auto& angle : curve.turns) {
            std::string field;
            if (!std::getline(fields, field, '\t') ||
                field.empty() || field.size() > 5) return false;
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(field.c_str(), &end, 10);
            if (errno != 0 || end == field.c_str() || *end ||
                parsed < -80 || parsed > 80) return false;
            angle = static_cast<int>(parsed);
        }
        std::string extra;
        if (std::getline(fields, extra, '\t')) return false;
        for (const auto& seen : *curves)
            if (seen.name == curve.name) return false;
        curves->push_back(std::move(curve));
    }
    return true;
}

void processMod(const SourceModStatus& mod, const std::string& dir) {
    const std::string root = mods::rootOf(mod.id);
    if (root.empty()) return;
    const std::string hash = fingerprint(mod.id, root);
    if (hash.empty()) {
        BL_WARN("csharp auto: fontes nao acessiveis %s", mod.id.c_str());
        return;
    }
    const std::string cacheDir = dir + "/auto-source-cache";
    if (mkdir(cacheDir.c_str(), 0700) != 0 && errno != EEXIST) return;
    const std::string prefix = cacheDir + "/" + mod.id + "-" + hash;
    std::string values, policies;
    bool cacheHit = readFile(prefix + ".values", &values, 16384) &&
                    readFile(prefix + ".curves", &policies, 16384);
    if (!cacheHit) {
        if (!writeFile(dir + "/source-compile-status.txt",
                       "state=auto-starting\nmodId=" + mod.id + "\n")) return;
        std::string error;
        if (!requestIsolatedMonoCompile(root, mod.id, error)) {
            BL_WARN("csharp auto: compilador nao iniciou para %s: %s",
                    mod.id.c_str(), error.c_str());
            return;
        }
        if (!waitForCompile(dir, mod) ||
            !readFile(dir + "/source-runtime-values.txt", &values, 16384) ||
            !readFile(dir + "/source-projectile-turns.txt", &policies, 16384)) {
            BL_WARN("csharp auto: compilacao nao validada para %s", mod.id.c_str());
            return;
        }
    }

    std::vector<CompiledPolicy> curves;
    if (!parseCurves(policies, mod, &curves)) {
        BL_WARN("csharp auto: curvas invalidas em %s", mod.id.c_str());
        return;
    }
    std::string error;
    if (!applyManagedSourceDefaults(mod.id, values, &error)) {
        BL_WARN("csharp auto: SetDefaults falhou para %s: %s",
                mod.id.c_str(), error.c_str());
        return;
    }
    if (!cacheHit) {
        writeFile(prefix + ".values", values);
        writeFile(prefix + ".curves", policies);
    }
    BL_INFO("csharp auto: %s aplicado, cache=%s, curvas=%zu",
            mod.id.c_str(), cacheHit ? "sim" : "novo", curves.size());
    for (const auto& curve : curves) {
        if (!stageManagedProjectileAutoCurve(mod.id, curve.name, curve.turns,
                                             &error)) {
            BL_WARN("csharp auto: IA %s/%s nao preparada: %s",
                    mod.id.c_str(), curve.name.c_str(), error.c_str());
        }
    }
    if (!cacheHit) std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

}

void bootstrapAutomaticSourceMods() {
    const std::string dir = checkpointDir();
    if (dir.empty()) return;
    for (const auto& mod : sourceModStatuses()) {
        if (!mod.loaded || mod.sourceFiles == 0 || mod.items == 0 ||
            mod.projectiles == 0 || !safeModId(mod.id)) continue;
        if (mod.id == kFixture) {
            BL_INFO("csharp auto: fixture de diagnostico mantida em modo manual");
            continue;
        }
        processMod(mod, dir);
    }
    if (managedProjectileAutoPending() > 0) {
        std::string error;
        if (!installManagedProjectileAutoHook(&error))
            BL_WARN("csharp auto: hook de IA indisponivel: %s", error.c_str());
    }
}

}
