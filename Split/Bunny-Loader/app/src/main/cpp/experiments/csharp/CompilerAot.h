#pragma once
#include "experiments/csharp/PortableSha256.h"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace bl::csharp {
// Private bundle ABI. Only APK-owned modules are accepted, never native code from mods.
struct CompilerAotModule { const char* assembly; const char* sha256; void* info; };
struct CompilerAotResult {
    void* handle = nullptr; // Keep registered code mapped for the entire Mono lifetime.
    int registered = 0;
    std::string reason;
    std::string detail;
};
inline std::string compilerAotHashFile(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return {};
    PortableSha256 hash;
    char buffer[16384];
    size_t total = 0, count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file))) {
        total += count;
        if (total > 64 * 1024 * 1024) { std::fclose(file); return {}; }
        hash.append(std::string_view(buffer, count));
    }
    const bool valid = !std::ferror(file) && total > 0;
    std::fclose(file);
    return valid ? hash.finish() : std::string{};
}
inline CompilerAotResult installCompilerAot(
    const char* bundlePath, const std::string& frameworkDir,
    const std::string& monoPath, void (*registerModule)(void**)) {
    CompilerAotResult result;
    if (!registerModule) { result.reason = "RegisterSymbolMissing"; return result; }
    void* bundle = dlopen(bundlePath, RTLD_NOW | RTLD_LOCAL);
    if (!bundle) {
        result.reason = "BundleUnavailable";
        const char* error = dlerror();
        result.detail = error ? std::string(error).substr(0, 512) : "dlopen returned null";
        return result;
    }
    using Version = int (*)();
    using RuntimeHash = const char* (*)();
    using Module = const CompilerAotModule* (*)(int);
    const auto version = reinterpret_cast<Version>(dlsym(bundle, "bunny_compiler_aot_abi"));
    const auto runtimeHash = reinterpret_cast<RuntimeHash>(dlsym(bundle, "bunny_compiler_aot_runtime_hash"));
    const auto count = reinterpret_cast<Version>(dlsym(bundle, "bunny_compiler_aot_count"));
    const auto module = reinterpret_cast<Module>(dlsym(bundle, "bunny_compiler_aot_module"));
    auto reject = [&](const std::string& reason) {
        result.reason = reason;
        dlclose(bundle);
        return result;
    };
    if (!version || !runtimeHash || !count || !module || version() != 1) {
        result.detail = "Expected=BundleAbi1 Symbols=abi,runtime_hash,count,module";
        return reject("BundleAbiMismatch");
    }
    const char* expectedRuntime = runtimeHash();
    const std::string actualRuntime = compilerAotHashFile(monoPath);
    if (!expectedRuntime || std::strlen(expectedRuntime) != 64 ||
        actualRuntime != expectedRuntime) {
        result.detail = "Path=" + monoPath + " Expected=" +
            (expectedRuntime ? expectedRuntime : "Null") + " Actual=" + actualRuntime;
        return reject("RuntimeHashMismatch");
    }
    const int size = count();
    if (size < 1 || size > 16) return reject("ModuleCountInvalid");
    std::vector<void*> infos;
    for (int index = 0; index < size; ++index) {
        const auto* item = module(index);
        if (!item || !item->assembly || !item->sha256 || !item->info)
            return reject("ModuleDescriptorInvalid");
        const std::string name = item->assembly;
        if (name.size() < 5 || name.size() > 128 || name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos || name.find("..") != std::string::npos ||
            name.substr(name.size() - 4) != ".dll") return reject("AssemblyNameInvalid");
        // .NET 8.0.23 aot-runtime.h: version is the first uint32 in MonoAotFileInfo.
        uint32_t format = 0;
        std::memcpy(&format, item->info, sizeof(format));
        if (format != 185) {
            result.detail = "Expected=185 Actual=" + std::to_string(format);
            return reject("AotFormatMismatch:" + name);
        }
        const std::string actual = compilerAotHashFile(frameworkDir + "/" + name);
        if (std::strlen(item->sha256) != 64 ||
            actual != item->sha256) {
            result.detail = "Assembly=" + name + " Expected=" + item->sha256 + " Actual=" + actual;
            return reject("AssemblyHashMismatch:" + name);
        }
        infos.push_back(item->info);
    }
    // Validate the complete set before the first irreversible runtime registration.
    for (void* info : infos) registerModule(static_cast<void**>(info));
    result.handle = bundle;
    result.registered = size;
    result.reason = "Registered";
    return result;
}
}
