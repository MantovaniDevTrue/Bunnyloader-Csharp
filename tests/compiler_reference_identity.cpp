#include "experiments/csharp/AutoSourceCache.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main() {
    namespace fs=std::filesystem;
    using namespace bl::csharp;
    const fs::path root=fs::temp_directory_path()/("bunny-ref-identity-"+std::to_string(getpid()));
    fs::create_directories(root);
    auto write=[&](const fs::path& path,const char* text) { std::ofstream(path)<<text; };
    for(const char* name:{"BunnySourceSdk.dll","BunnySourceCompiler.dll","BunnyManagedProbe.dll",
        "Microsoft.CodeAnalysis.dll","Microsoft.CodeAnalysis.CSharp.dll","System.Private.CoreLib.dll","System.Runtime.dll"})
        write(root/name,"runtime");
    auto original=auto_cache::runtimeIdentity(root.string());assert(auto_cache::digest(original));
    fs::create_directories(root/"compiler-references");
    auto reference=root/"compiler-references/System.Runtime.dll";
    write(reference,"reference-A");auto a=auto_cache::runtimeIdentity(root.string());assert(a!=original);
    write(reference,"reference-B");assert(auto_cache::runtimeIdentity(root.string())!=a);
    fs::remove(reference);fs::create_symlink(root/"System.Runtime.dll",reference);
    assert(auto_cache::runtimeIdentity(root.string()).empty());
    fs::remove_all(root/"compiler-references");assert(auto_cache::runtimeIdentity(root.string())==original);
    fs::remove_all(root);std::cout<<"COMPILER_REFERENCE_CONTENT_AND_SYMLINK_INVALIDATION_PASS\n";
}
