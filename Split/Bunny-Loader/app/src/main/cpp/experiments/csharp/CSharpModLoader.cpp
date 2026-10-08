#include "experiments/csharp/CSharpModLoader.h"

#include "content/projectiles/ModProjectiles.h"
#include "core/Config.h"
#include "core/Log.h"
#include "mods/ModLoader.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace bl::csharpmods {

namespace {

constexpr int kNativeModAbi = 1;
constexpr int kHostApiVersion = 1;

using AbiVersionFn = int (*)();
using HostApiVersionFn = int (*)();
using InitializeFn = int (*)(int);
using ProjectileTickFn = int (*)(bl::csharp::HostProjectileState*,
                                 const bl::csharp::NpcSample*,
                                 int,
                                 float,
                                 float);

struct NativeMod {
    LoadedNativeMod info;
    void* handle = nullptr;
    ProjectileTickFn projectileTick = nullptr;
};

std::mutex g_mutex;
std::vector<NativeMod> g_mods;

bool fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

std::string processPackage() {
    FILE* f = std::fopen("/proc/self/cmdline", "rb");
    if (!f) return {};
    char buf[256]{};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    std::string name(buf, strnlen(buf, n));
    const size_t colon = name.find(':');
    if (colon != std::string::npos) name.resize(colon);
    return name;
}

std::string safeName(const std::string& id) {
    std::string out;
    out.reserve(id.size());
    for (unsigned char c : id) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out += static_cast<char>(c);
        } else {
            out += '_';
        }
    }
    if (out.empty()) out = "mod";
    return out;
}

bool ensureDir(const std::string& path) {
    if (mkdir(path.c_str(), 0700) == 0) return true;
    return errno == EEXIST;
}

bool copyFile(const std::string& from, const std::string& to) {
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) {
        std::fclose(in);
        return false;
    }

    char buf[64 * 1024];
    bool ok = true;
    while (true) {
        const size_t n = std::fread(buf, 1, sizeof(buf), in);
        if (n && std::fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
        if (n < sizeof(buf)) {
            if (std::ferror(in)) ok = false;
            break;
        }
    }

    std::fclose(in);
    if (std::fclose(out) != 0) ok = false;
    if (ok) chmod(to.c_str(), 0700);
    else unlink(to.c_str());
    return ok;
}

std::string cachePath(const std::string& modId, const std::string& sourcePath) {
    const std::string package = processPackage();
    if (package.empty()) return {};

    const std::string codeCache = "/data/user/0/" + package + "/code_cache";
    const std::string dir = codeCache + "/bunny-csharp";
    if (!ensureDir(codeCache) || !ensureDir(dir)) return {};

    struct stat st{};
    unsigned long long stamp = 0;
    if (stat(sourcePath.c_str(), &st) == 0) {
        stamp = static_cast<unsigned long long>(st.st_size) ^
                (static_cast<unsigned long long>(st.st_mtime) << 17);
    }

    char suffix[32];
    std::snprintf(suffix, sizeof(suffix), "-%llx.so", stamp);
    return dir + "/" + safeName(modId) + suffix;
}

void* openNativeLibrary(const std::string& id,
                        const std::string& source,
                        std::string* loadedPath) {
    dlerror();
    void* direct = dlopen(source.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (direct) {
        *loadedPath = source;
        return direct;
    }

    const char* directError = dlerror();
    const std::string cache = cachePath(id, source);
    if (cache.empty()) {
        BL_ERROR("csharp mod %s: dlopen direto falhou (%s) e cache interno indisponivel",
                 id.c_str(), directError ? directError : "?");
        return nullptr;
    }

    if (!fileExists(cache) && !copyFile(source, cache)) {
        BL_ERROR("csharp mod %s: nao consegui copiar %s para %s",
                 id.c_str(), source.c_str(), cache.c_str());
        return nullptr;
    }

    dlerror();
    void* handle = dlopen(cache.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* cachedError = dlerror();
        BL_ERROR("csharp mod %s: dlopen falhou: direto=%s; cache=%s",
                 id.c_str(),
                 directError ? directError : "?",
                 cachedError ? cachedError : "?");
        return nullptr;
    }

    *loadedPath = cache;
    BL_INFO("csharp mod %s: biblioteca copiada para cache executavel", id.c_str());
    return handle;
}

template <typename T>
T symbol(void* handle, const char* name) {
    return reinterpret_cast<T>(dlsym(handle, name));
}

void tryLoad(const bl::mods::LoadedMod& mod) {
    const std::string source = mod.dir + "/native/arm64-v8a/libmod.so";
    if (!fileExists(source)) return;

    NativeMod native;
    native.info.id = mod.id;
    native.info.sourcePath = source;
    native.handle = openNativeLibrary(mod.id, source, &native.info.loadedPath);
    if (!native.handle) return;

    const auto abi = symbol<AbiVersionFn>(native.handle, "bunny_mod_abi_version");
    const auto hostApi = symbol<HostApiVersionFn>(native.handle, "bunny_mod_host_api_version");
    const auto initialize = symbol<InitializeFn>(native.handle, "bunny_mod_initialize");
    native.projectileTick =
        symbol<ProjectileTickFn>(native.handle, "bunny_mod_projectile_tick");

    if (!abi || !hostApi || !initialize) {
        BL_ERROR("csharp mod %s: exports obrigatorios ausentes", mod.id.c_str());
        dlclose(native.handle);
        return;
    }

    native.info.abiVersion = abi();
    native.info.hostApiVersion = hostApi();
    if (native.info.abiVersion != kNativeModAbi) {
        BL_ERROR("csharp mod %s: ABI %d, Bunny espera %d",
                 mod.id.c_str(), native.info.abiVersion, kNativeModAbi);
        dlclose(native.handle);
        return;
    }
    if (native.info.hostApiVersion > kHostApiVersion || native.info.hostApiVersion <= 0) {
        BL_ERROR("csharp mod %s: Host API %d nao suportada (Bunny=%d)",
                 mod.id.c_str(), native.info.hostApiVersion, kHostApiVersion);
        dlclose(native.handle);
        return;
    }
    if (initialize(kHostApiVersion) == 0) {
        BL_ERROR("csharp mod %s: inicializacao recusada", mod.id.c_str());
        dlclose(native.handle);
        return;
    }

    native.info.projectileBehavior = native.projectileTick != nullptr;
    native.info.loaded = true;
    BL_INFO("csharp mod %s: NativeAOT carregado (ABI %d, Host API %d, projectile=%s)",
            mod.id.c_str(), native.info.abiVersion, native.info.hostApiVersion,
            native.info.projectileBehavior ? "sim" : "nao");
    g_mods.push_back(std::move(native));
}

}

void loadAll() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_mods.empty()) return;

    for (size_t i = 0; i < bl::mods::loadedCount(); ++i) {
        const bl::mods::LoadedMod* mod = bl::mods::get(static_cast<uint16_t>(i));
        if (!mod || !mod->enabled || !mod->loaded || mod->dir == "(builtin)") continue;
        tryLoad(*mod);
    }

    BL_INFO("csharp mods: %zu biblioteca(s) NativeAOT externa(s) carregada(s)", g_mods.size());
}

size_t loadedCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_mods.size();
}

std::vector<LoadedNativeMod> snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<LoadedNativeMod> out;
    out.reserve(g_mods.size());
    for (const NativeMod& mod : g_mods) out.push_back(mod.info);
    return out;
}

bool hasProjectileBehavior() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const NativeMod& mod : g_mods) {
        if (mod.info.loaded && mod.projectileTick) return true;
    }
    return false;
}

bool hasOwnedProjectileBehavior(int projectileType) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const NativeMod& mod : g_mods) {
        if (mod.info.loaded && mod.projectileTick &&
            bl::runtime::modProjectileBelongsTo(projectileType, mod.info.id)) {
            return true;
        }
    }
    return false;
}

bool dispatchOwnedProjectile(int projectileType,
                             bl::csharp::HostProjectileState* state,
                             const bl::csharp::NpcSample* npcs,
                             int count,
                             float playerX,
                             float playerY) {
    if (!state || !npcs || count <= 0 || state->type != projectileType) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (NativeMod& mod : g_mods) {
        if (!mod.info.loaded || !mod.projectileTick ||
            !bl::runtime::modProjectileBelongsTo(projectileType, mod.info.id)) continue;
        return mod.projectileTick(state, npcs, count, playerX, playerY) != 0;
    }
    return false;
}

bool dispatchProjectile(bl::csharp::HostProjectileState* state,
                        const bl::csharp::NpcSample* npcs,
                        int count,
                        float playerX,
                        float playerY) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (NativeMod& mod : g_mods) {
        if (!mod.info.loaded || !mod.projectileTick) continue;
        if (mod.projectileTick(state, npcs, count, playerX, playerY) != 0) return true;
    }
    return false;
}

}
