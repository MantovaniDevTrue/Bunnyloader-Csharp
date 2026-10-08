#include "experiments/csharp/ManagedRuntime.h"

#include <cstdio>
#include <ctime>
#include <cstring>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace bl::csharp {
namespace {

using InitFn = void* (*)(const char*);
using AssemblyPathFn = void (*)(const char*);
using AttachFn = void* (*)(void*);
using OpenFn = void* (*)(void*, const char*);
using ImageFn = void* (*)(void*);
using ClassFn = void* (*)(void*, const char*, const char*);
using MethodFn = void* (*)(void*, const char*, int);
using InvokeFn = void* (*)(void*, void*, void**, void**);
using UnboxFn = void* (*)(void*);
using NewStringFn = void* (*)(void*, const char*);

struct MonoApi {
    InitFn initialize = nullptr;
    AssemblyPathFn setAssembliesPath = nullptr;
    AttachFn attach = nullptr;
    OpenFn openAssembly = nullptr;
    ImageFn image = nullptr;
    ClassFn findClass = nullptr;
    MethodFn findMethod = nullptr;
    InvokeFn invoke = nullptr;
    UnboxFn unbox = nullptr;
    NewStringFn newString = nullptr;
};

std::mutex g_mutex;
ManagedRuntimeStatus g_status;
MonoApi g_api;
void* g_library = nullptr;
void* g_domain = nullptr;
std::string g_checkpointPath;
void* g_liveAiMethod = nullptr;

template <typename T>
T resolve(const char* name) {
    return reinterpret_cast<T>(dlsym(g_library, name));
}

void saveCheckpoint() {
    if (g_checkpointPath.empty()) return;
    const std::string tmp = g_checkpointPath + ".tmp";
    FILE* file = std::fopen(tmp.c_str(), "wb");
    if (!file) return;
    std::fprintf(file, "timestampUnix=%lld\npid=%ld\nphase=%s\n"
                       "attempted=%d\nlibraryLoaded=%d\nsymbolsReady=%d\n"
                       "domainInitialized=%d\nmethodExecuted=%d\nresult=%d\nerror=%s\n",
                 static_cast<long long>(std::time(nullptr)), static_cast<long>(getpid()),
                 g_status.phase.c_str(), g_status.attempted,
                 g_status.libraryLoaded, g_status.symbolsReady,
                 g_status.domainInitialized, g_status.methodExecuted,
                 g_status.result, g_status.error.c_str());
    const bool flushed = std::fflush(file) == 0;
    if (flushed) fsync(fileno(file));
    std::fclose(file);
    if (flushed) std::rename(tmp.c_str(), g_checkpointPath.c_str());
}

void trace(const char* phase, const char* detail = nullptr) {
    g_status.phase = phase;
    saveCheckpoint();
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                        "mono phase=%s detail=%s", phase, detail ? detail : "");
#else
    std::fprintf(stderr, "mono phase=%s detail=%s\n", phase, detail ? detail : "");
#endif
}

ManagedRuntimeStatus failure(const char* phase, const std::string& detail) {
    g_status.error = detail.substr(0, 512);
    g_status.methodExecuted = false;
    trace(phase, g_status.error.c_str());
    return g_status;
}

bool validAssembly(const std::string& path) {
    if (path.empty() || path.size() > 2048 || path.find("..") != std::string::npos)
        return false;
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
           st.st_size > 0 && st.st_size <= 8 * 1024 * 1024;
}

bool loadLibrary() {
    if (g_library) return true;
    trace("opening-library");
    g_library = dlopen("libmonosgen-2.0.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_library) {
        const char* message = dlerror();
        failure("library-missing", message ? message : "libmonosgen-2.0.so indisponivel");
        return false;
    }
    g_status.libraryLoaded = true;
    trace("resolving-symbols");
    g_api.initialize = resolve<InitFn>("mono_jit_init");
    g_api.setAssembliesPath = resolve<AssemblyPathFn>("mono_set_assemblies_path");
    g_api.attach = resolve<AttachFn>("mono_thread_attach");
    g_api.openAssembly = resolve<OpenFn>("mono_domain_assembly_open");
    g_api.image = resolve<ImageFn>("mono_assembly_get_image");
    g_api.findClass = resolve<ClassFn>("mono_class_from_name");
    g_api.findMethod = resolve<MethodFn>("mono_class_get_method_from_name");
    g_api.invoke = resolve<InvokeFn>("mono_runtime_invoke");
    g_api.unbox = resolve<UnboxFn>("mono_object_unbox");
    g_api.newString = resolve<NewStringFn>("mono_string_new");
    if (!g_api.initialize || !g_api.attach || !g_api.openAssembly || !g_api.image ||
        !g_api.findClass || !g_api.findMethod || !g_api.invoke || !g_api.unbox) {
        failure("abi-incompatible", "Mono Embedding API incompleta");
        dlclose(g_library);
        g_library = nullptr;
        g_api = {};
        g_status.libraryLoaded = false;
        return false;
    }
    g_status.symbolsReady = true;
    trace("symbols-ready");
    return true;
}

}

void setManagedCheckpointPath(const std::string& path) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (path.size() <= 2048 && path.find("..") == std::string::npos)
        g_checkpointPath = path;
}

ManagedRuntimeStatus managedRuntimeStatus() {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_status;
}

ManagedRuntimeStatus inspectManagedRuntime() {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.attempted = true;
    g_status.error.clear();
    if (loadLibrary() && !g_status.domainInitialized)
        trace("symbols-ready");
    return g_status;
}

ManagedRuntimeStatus probeManagedRuntime(const std::string& assemblyPath) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.attempted = true;
    g_status.methodExecuted = false;
    g_status.result = 0;
    g_status.error.clear();
    trace("checking-assembly");
    if (!validAssembly(assemblyPath))
        return failure("invalid-assembly", "assembly de teste ausente ou fora do limite");

    if (!loadLibrary()) return g_status;

    if (!g_domain) {
        const auto slash = assemblyPath.find_last_of('/');
        if (g_api.setAssembliesPath && slash != std::string::npos) {
            trace("setting-assembly-path");
            g_api.setAssembliesPath(assemblyPath.substr(0, slash).c_str());
        }
#ifdef __ANDROID__
        trace("checking-system-native");
        void* systemNative = dlopen("libSystem.Native.so", RTLD_NOW | RTLD_LOCAL);
        if (!systemNative) {
            const char* message = dlerror();
            return failure("system-native-missing",
                           message ? message : "libSystem.Native.so indisponivel");
        }
        trace("system-native-ready");
#endif
        trace("initializing-domain");
        g_domain = g_api.initialize("BunnyCSharp");
        if (!g_domain)
            return failure("domain-failed", "mono_jit_init retornou nulo");
    }
    g_status.domainInitialized = true;
    trace("attaching-thread");
    if (!g_api.attach(g_domain))
        return failure("attach-failed", "mono_thread_attach retornou nulo");

    trace("opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
    if (!assembly) return failure("assembly-failed", "mono_domain_assembly_open falhou");

    trace("resolving-method");
    void* image = g_api.image(assembly);
    void* klass = image ? g_api.findClass(image, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "Run", 2) : nullptr;
    if (!method) return failure("method-missing", "BunnyLoader.ManagedProbe.Probe.Run(int,int) ausente");

    int a = 19;
    int b = 23;
    void* parameters[] = {&a, &b};
    void* exception = nullptr;
    trace("invoking-method");
    void* result = g_api.invoke(method, nullptr, parameters, &exception);
    if (exception) return failure("managed-exception", "Run lancou uma excecao gerenciada");
    if (!result) return failure("null-result", "Run nao retornou objeto");

    void* unboxed = g_api.unbox(result);
    if (!unboxed) return failure("unbox-failed", "mono_object_unbox retornou nulo");

    int value = 0;
    std::memcpy(&value, unboxed, sizeof(value));
    g_status.result = value;
    if (value != 42) return failure("result-mismatch", "Run retornou resultado inesperado");

    g_status.methodExecuted = true;
    trace("verified");
    return g_status;
}

ManagedRuntimeStatus probeManagedCallbacks(const std::string& assemblyPath, int count) {
    std::lock_guard<std::mutex> guard(g_mutex);
    g_status.callbacksExecuted = 0;
    g_status.error.clear();
    if (!g_domain || !g_status.domainInitialized)
        return failure("callback-runtime-not-ready", "Executar managedGameProbe antes dos callbacks");
    if (!validAssembly(assemblyPath))
        return failure("callback-assembly-missing", "BunnyManagedProbe.dll ausente");
    if (count < 1 || count > 120)
        return failure("callback-count-invalid", "Quantidade deve ser entre 1 e 120");
    if (!g_api.attach(g_domain))
        return failure("callback-attach-failed", "mono_thread_attach retornou nulo");

    trace("callback-opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
    if (!assembly)
        return failure("callback-assembly-failed", "Nao foi possivel carregar BunnyManagedProbe");
    void* img = g_api.image(assembly);
    void* klass = img ? g_api.findClass(img, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "AI", 2) : nullptr;
    if (!method)
        return failure("callback-method-missing", "Probe.AI(int,int) nao encontrado");

    int state = 0;
    int expected = 0;
    trace("invoking-ai-callbacks");
    for (int tick = 0; tick < count; ++tick) {
        int tickArg = tick;
        int stateArg = state;
        void* args[] = {&tickArg, &stateArg};
        void* exception = nullptr;
        void* boxed = g_api.invoke(method, nullptr, args, &exception);
        if (exception || !boxed)
            return failure("callback-invoke-failed", "Probe.AI lancou excecao ou retornou nulo");
        void* value = g_api.unbox(boxed);
        if (!value)
            return failure("callback-unbox-failed", "Probe.AI retornou valor invalido");
        std::memcpy(&state, value, sizeof(state));
        expected += (tick % 7) - 3;
        ++g_status.callbacksExecuted;
        if (state != expected)
            return failure("callback-mismatch", "Probe.AI retornou resultado incorreto");
    }
    g_status.result = state;
    g_status.methodExecuted = true;
    trace("callback-verified");
    return g_status;
}

ManagedSourceCompileResult compileManagedSource(
    const std::string& probeAssembly,
    const std::string& compilerAssembly,
    const std::string& modRoot,
    const std::string& outputAssembly,
    const std::string& diagnosticsPath) {
    ManagedSourceCompileResult out;
    if (modRoot.empty() || outputAssembly.empty() || diagnosticsPath.empty() ||
        modRoot.size() > 2048 || outputAssembly.size() > 2048 ||
        diagnosticsPath.size() > 2048 ||
        modRoot.find("..") != std::string::npos ||
        outputAssembly.find("..") != std::string::npos ||
        diagnosticsPath.find("..") != std::string::npos) {
        out.phase = "compile-path-invalid";
        out.error = "Diretorio do mod ou saida invalida";
        return out;
    }
    const auto initialized = probeManagedRuntime(probeAssembly);
    if (!initialized.domainInitialized || !initialized.methodExecuted) {
        out.phase = "compile-mono-unavailable";
        out.error = initialized.error;
        return out;
    }

    std::lock_guard<std::mutex> guard(g_mutex);
    if (!validAssembly(compilerAssembly) || !g_api.newString) {
        out.phase = "compile-host-unavailable";
        out.error = "Roslyn assembly ou mono_string_new indisponivel";
        trace("compile-host-unavailable", out.error.c_str());
        return out;
    }
    trace("compile-opening-assembly");
    void* assembly = g_api.openAssembly(g_domain, compilerAssembly.c_str());
    void* image = assembly ? g_api.image(assembly) : nullptr;
    void* klass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "Compiler") : nullptr;
    void* method = klass ? g_api.findMethod(klass, "Compile", 4) : nullptr;
    if (!method) {
        out.phase = "compile-method-missing";
        out.error = "Compiler.Compile(string,string,string,string) indisponivel";
        trace("compile-method-missing", out.error.c_str());
        return out;
    }
    void* preflightClass = image ? g_api.findClass(
        image, "BunnyLoader.SourceCompiler", "Preflight") : nullptr;
    void* ping = preflightClass ? g_api.findMethod(preflightClass, "Ping", 1) : nullptr;
    if (!ping) {
        out.phase = "compile-preflight-missing";
        out.error = "Preflight.Ping(string) indisponivel";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    trace("compile-preflight-start");
    void* pingArgument = g_api.newString(g_domain, diagnosticsPath.c_str());
    if (!pingArgument) {
        out.phase = "compile-preflight-argument-failed";
        trace(out.phase.c_str());
        return out;
    }
    void* pingArgs[] = { pingArgument };
    void* pingException = nullptr;
    void* pingResult = g_api.invoke(ping, nullptr, pingArgs, &pingException);
    if (pingException || !pingResult || !g_api.unbox(pingResult)) {
        out.phase = "compile-preflight-exception";
        out.error = "Falha antes de carregar a rotina Roslyn";
        trace(out.phase.c_str(), out.error.c_str());
        return out;
    }
    int pingValue = 0;
    std::memcpy(&pingValue, g_api.unbox(pingResult), sizeof(pingValue));
    if (pingValue != 42) {
        out.phase = "compile-preflight-invalid-result";
        trace(out.phase.c_str());
        return out;
    }
    trace("compile-preflight-verified");

    const auto slash = compilerAssembly.find_last_of('/');
    if (slash == std::string::npos) {
        out.phase = "compile-framework-invalid";
        return out;
    }
    const std::string frameworkDir = compilerAssembly.substr(0, slash);
    void* argsManaged[] = {
        g_api.newString(g_domain, modRoot.c_str()),
        g_api.newString(g_domain, outputAssembly.c_str()),
        g_api.newString(g_domain, frameworkDir.c_str()),
        g_api.newString(g_domain, diagnosticsPath.c_str())
    };
    if (!argsManaged[0] || !argsManaged[1] || !argsManaged[2] || !argsManaged[3]) {
        out.phase = "compile-arguments-failed";
        return out;
    }
    void* args[] = {
        argsManaged[0], argsManaged[1], argsManaged[2], argsManaged[3]
    };
    void* exception = nullptr;
    trace("compiling-source");
    void* boxed = g_api.invoke(method, nullptr, args, &exception);
    if (exception || !boxed) {
        out.phase = "compile-managed-exception";
        out.error = "Excecao no compilador gerenciado, consultar logcat";
        trace("compile-managed-exception", out.error.c_str());
        return out;
    }
    void* raw = g_api.unbox(boxed);
    if (!raw) {
        out.phase = "compile-return-invalid";
        trace("compile-return-invalid");
        return out;
    }
    std::memcpy(&out.result, raw, sizeof(out.result));
    out.phase = out.result > 0 ? "compile-verified" : "compile-failed";
    trace(out.phase.c_str());
    return out;
}

bool invokeManagedProjectileAI(int tick, int state, int* nextState) {
    if (!nextState || tick < 0 || tick >= 120) return false;
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_domain || !g_status.domainInitialized || !g_api.attach ||
        !g_api.invoke || !g_api.unbox) return false;
    if (!g_api.attach(g_domain)) return false;

    if (!g_liveAiMethod) {
        const size_t slash = g_checkpointPath.find_last_of('/');
        if (slash == std::string::npos) return false;
        const std::string assemblyPath =
            g_checkpointPath.substr(0, slash + 1) + "BunnyManagedProbe.dll";
        if (!validAssembly(assemblyPath)) return false;
        void* assembly = g_api.openAssembly(g_domain, assemblyPath.c_str());
        void* img = assembly ? g_api.image(assembly) : nullptr;
        void* klass = img ? g_api.findClass(img, "BunnyLoader.ManagedProbe", "Probe") : nullptr;
        g_liveAiMethod = klass ? g_api.findMethod(klass, "AI", 2) : nullptr;
        if (!g_liveAiMethod) return false;
    }

    int tickArg = tick;
    int stateArg = state;
    void* args[] = {&tickArg, &stateArg};
    void* exception = nullptr;
    void* boxed = g_api.invoke(g_liveAiMethod, nullptr, args, &exception);
    if (exception || !boxed) return false;
    void* unboxed = g_api.unbox(boxed);
    if (!unboxed) return false;
    int next = 0;
    std::memcpy(&next, unboxed, sizeof(next));
    if (next != state + (tick % 7) - 3) return false;
    *nextState = next;
    return true;
}

}
