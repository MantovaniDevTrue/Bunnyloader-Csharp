#include <cassert>
#include <filesystem>
#include <iostream>
#include "../app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp"

int main(int argc,char** argv) {
    assert(argc==2);
    using namespace bl::csharp;
    namespace fs=std::filesystem;
    const fs::path root=argv[1];fs::create_directories(root/"content");fs::create_directories(root/"runtime");
    auto hash=[](std::string_view text){PortableSha256 h;h.append(text);return h.finish();};
    assert(hash("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(hash("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")==
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    PortableSha256 million;for(int n=0;n<1000000;++n)million.append("a");
    assert(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    const std::string binary(257,'\xff');PortableSha256 partial;partial.append(std::string_view(binary).substr(0,63));
    partial.append(std::string_view(binary).substr(63));assert(partial.finish()==hash(binary));
    PortableSha256 framedA,framedB;framedA.field("ab");framedA.field("c");framedB.field("a");framedB.field("bc");
    assert(framedA.finish()!=framedB.finish());
    const std::string runtime=(root/"runtime").string();
    assert(auto_cache::runtimeIdentity(runtime).empty());
    const std::vector<std::string> names={"BunnySourceSdk.dll","BunnySourceCompiler.dll","BunnyManagedProbe.dll",
        "Microsoft.CodeAnalysis.dll","Microsoft.CodeAnalysis.CSharp.dll","System.Private.CoreLib.dll","System.Runtime.dll"};
    for(const auto& name:names)assert(writeText(runtime+"/"+name,"runtime:"+name));
    const auto identity=auto_cache::runtimeIdentity(runtime);assert(auto_cache::digest(identity));
    for(const auto& name:names) {
        const auto file=runtime+"/"+name,original=readText(file,1024);auto changed=original;changed[0]^=1;
        assert(writeText(file,changed));assert(auto_cache::runtimeIdentity(runtime)!=identity);
        assert(writeText(file,original));assert(auto_cache::runtimeIdentity(runtime)==identity);
        fs::remove(file);assert(auto_cache::runtimeIdentity(runtime).empty());assert(writeText(file,original));
    }
    assert(writeText(runtime+"/System.Linq.dll","optional-reference"));assert(auto_cache::runtimeIdentity(runtime)!=identity);
    fs::remove(runtime+"/System.Linq.dll");assert(auto_cache::runtimeIdentity(runtime)==identity);
    fs::create_symlink(runtime+"/BunnySourceSdk.dll",runtime+"/Extra.dll");assert(auto_cache::runtimeIdentity(runtime).empty());
    fs::remove(runtime+"/Extra.dll");
    fs::create_directories(runtime+"/compiler-references");
    assert(writeText(runtime+"/compiler-references/System.Runtime.dll","compile-reference"));
    const auto compactIdentity=auto_cache::runtimeIdentity(runtime);
    assert(compactIdentity!=identity);
    assert(writeText(runtime+"/compiler-references/System.Runtime.dll","compile-referencf"));
    assert(auto_cache::runtimeIdentity(runtime)!=compactIdentity);
    fs::remove_all(runtime+"/compiler-references");
    assert(auto_cache::runtimeIdentity(runtime)==identity);
    const auto source=(root/"content/main.cs").string();assert(writeText(source,"class Material {}"));
    const auto key=fingerprint("mod",root.string(),identity);assert(auto_cache::digest(key));
    assert(fingerprint("other",root.string(),identity)!=key);
    assert(fingerprint("mod",root.string(),hash("new-sdk"))!=key);
    assert(writeText(source,"class Materiam {}"));assert(fingerprint("mod",root.string(),identity)!=key);
    assert(writeText(source,"class Material {}"));fs::rename(source,root/"content/renamed.cs");
    assert(fingerprint("mod",root.string(),identity)!=key);fs::rename(root/"content/renamed.cs",source);
    assert(fingerprint("mod",root.string(),identity)==key);
    const std::string dll=(root/"mod.dll").string(),cache=(root/"mod.cache").string();
    const std::string defaults="BUNNY_MANAGED_DEFAULTS_V2\nITEM\tMaterial\t12\n";
    const std::string curves="BUNNY_PROJECTILE_TURNS_V1\n",combat="BUNNY_COMBAT_POLICIES_V1\n0\n";
    const std::string assembly="fake-managed-assembly";assert(writeText(dll,assembly));
    saveCache(cache,"mod",key,dll,defaults,curves,combat);
    std::string d="unchanged",t=d,p=d;
    auto read=[&]{return readCache(cache,"mod",key,dll,&d,&t,&p);};
    assert(read()&&d==defaults&&t==curves&&p==combat);
    const auto lazyWork=(root/"lazy-worker-framework").string();
    fs::create_directories(lazyWork);
    std::string coldRuntimeIdentity;
    std::string coldKey=fingerprint("mod",root.string(),coldRuntimeIdentity);
    std::string coldCacheFile=(root/"missing-first-boot.cache").string();
    assert(!recoverRuntimeCacheIdentityAfterCompile(
        lazyWork,root.string(),"mod",root.string(),&coldRuntimeIdentity,&coldKey,&coldCacheFile));
    for(const auto& name:names)assert(writeText(lazyWork+"/"+name,"runtime:"+name));
    assert(recoverRuntimeCacheIdentityAfterCompile(
        lazyWork,root.string(),"mod",root.string(),&coldRuntimeIdentity,&coldKey,&coldCacheFile));
    assert(auto_cache::digest(coldRuntimeIdentity));
    assert(coldKey==fingerprint("mod",root.string(),coldRuntimeIdentity));
    assert(coldCacheFile==root.string()+"/mod-"+coldKey+".cache");
    saveCache(coldCacheFile,"mod",coldKey,dll,defaults,curves,combat);
    std::string coldDefaults,coldCurves,coldCombat;
    assert(readCache(coldCacheFile,"mod",coldKey,dll,&coldDefaults,&coldCurves,&coldCombat));
    assert(coldDefaults==defaults&&coldCurves==curves&&coldCombat==combat);
    assert(!recoverRuntimeCacheIdentityAfterCompile(
        lazyWork,root.string(),"mod",root.string(),&coldRuntimeIdentity,&coldKey,&coldCacheFile));
    const auto previousColdKey=coldKey;
    assert(writeText(lazyWork+"/System.Runtime.dll","runtime:System.Runtime.dll:modified"));
    assert(recoverRuntimeCacheIdentityAfterCompile(
        lazyWork,root.string(),"mod",root.string(),&coldRuntimeIdentity,&coldKey,&coldCacheFile));
    assert(coldKey!=previousColdKey);
    assert(coldCacheFile==root.string()+"/mod-"+coldKey+".cache");
    saveCache(coldCacheFile,"mod",coldKey,dll,defaults,curves,combat);
    assert(readCache(coldCacheFile,"mod",coldKey,dll,&coldDefaults,&coldCurves,&coldCombat));
    std::string reason;
    assert(readCache(cache,"mod",key,dll,&d,&t,&p,&reason)&&reason=="Hit");
    assert(!readCache(cache,"mod",hash("different-key"),dll,&d,&t,&p,&reason));
    assert(reason=="SourceFingerprintChanged");
    assert(!readCache(cache+"-not-found","mod",key,dll,&d,&t,&p,&reason));
    assert(reason=="CacheFileMissingOrUnreadable");
    const auto original=readText(cache,8192);assert(!original.empty());
    // Change a numeric descriptor while keeping its framing and marker valid.
    auto corrupted=original;corrupted[corrupted.find("12\n")]='9';assert(writeText(cache,corrupted));
    d=t=p="unchanged";assert(!readCache(cache,"mod",key,dll,&d,&t,&p,&reason));
    assert(reason=="SectionsChecksumChanged");
    assert(d=="unchanged"&&t==d&&p==d);
    assert(writeText(cache,original));assert(read());
    for(size_t n=0;n<original.size();++n){assert(writeText(cache,original.substr(0,n)));assert(!read());}
    assert(writeText(cache,original+"x"));assert(!read());
    assert(writeText(cache,"BUNNY_AUTO_SOURCE_OLD\nmod\n"+key+"\n"+defaults+curves+combat));assert(!read());
    assert(writeText(cache,original));assert(!readCache(cache,"other",key,dll,&d,&t,&p));
    assert(!readCache(cache,"mod",hash("new-key"),dll,&d,&t,&p));
    assert(writeText(dll,"fake-managed-assemblz"));assert(!read());
    fs::remove(dll);assert(!read());assert(writeText(dll,assembly));assert(read());
    assert(writeText(dll,"x"));assert(auto_cache::fileHash(dll,0).empty());
    fs::remove(dll);fs::create_symlink(source,dll);assert(!read());fs::remove(dll);
    assert(writeText(dll,assembly));
    // Bad sizes, markers and checksums must not publish partially decoded sections.
    const auto dllHash=auto_cache::fileHash(dll,kSourceMaxAssemblyBytes);
    assert(auto_cache::encode(kCacheVersion,"mod",key,dllHash,"invalid",curves,combat).empty());
    assert(auto_cache::encode(kCacheVersion,"mod",key,"",defaults,curves,combat).empty());
    auto invalid=original;const auto sizeStart=invalid.find('\n',invalid.find(key))+1;
    const auto sizeEnd=invalid.find('\n',sizeStart);invalid.replace(sizeStart,sizeEnd-sizeStart,"18446744073709551615 0 0");
    assert(writeText(cache,invalid));assert(!read());
    invalid=original;invalid.replace(sizeStart,sizeEnd-sizeStart,"1 2 3 extra");assert(writeText(cache,invalid));assert(!read());
    assert(writeText(cache,original));assert(read());
    saveCache(cache,"mod",key,dll,defaults,"",combat);assert(read()&&t.empty());
    // Exercise the real outer cache writer and reader at the expanded assembly
    // budget. These are byte/hash fixtures; assembly validity is checked by Mono.
    for(const auto bytes:{size_t{8*1024*1024+1},kSourceMaxAssemblyBytes}) {
        fs::resize_file(dll,bytes);
        assert(saveCache(cache,"mod",key,dll,defaults,curves,combat));
        assert(read()&&d==defaults&&t==curves&&p==combat);
        std::fstream changed(dll,std::ios::in|std::ios::out|std::ios::binary);
        changed.seekp(static_cast<std::streamoff>(bytes-1));changed.put('x');changed.close();
        assert(!read());
    }
    const auto lastValidCache=readText(cache,8192);
    fs::resize_file(dll,kSourceMaxAssemblyBytes+1);
    assert(!saveCache(cache,"mod",key,dll,defaults,curves,combat));
    assert(readText(cache,8192)==lastValidCache);
    assert(!read());
    assert(writeText(dll,assembly));
    assert(saveCache(cache,"mod",key,dll,defaults,curves,combat)&&read());
    std::cout<<"AUTO_SOURCE_VERIFIED_CACHE_SHA256_RUNTIME_SOURCES_DLL_CORRUPTION_AND_MIGRATION_PASS\n";
}
