#pragma once
#include "experiments/csharp/PortableSha256.h"
#include "experiments/csharp/SourceLimits.h"
#include <algorithm>
#include <array>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace bl::csharp::auto_cache {
inline constexpr const char* envelope="BUNNY_AUTO_SOURCE_VERIFIED_V2";
inline bool digest(const std::string& value) {
    return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
inline std::string fileHash(const std::string& path,size_t limit) {
    struct stat info{};
    if(lstat(path.c_str(),&info)!=0||!S_ISREG(info.st_mode)||info.st_size<=0||
       static_cast<uintmax_t>(info.st_size)>limit)return {};
    std::ifstream input(path,std::ios::binary);if(!input)return {};
    PortableSha256 hash;std::array<char,64*1024> buffer{};size_t total=0;
    while(input) {
        input.read(buffer.data(),buffer.size());const auto count=input.gcount();
        total+=static_cast<size_t>(count);if(total>limit)return {};
        hash.append(std::string_view(buffer.data(),static_cast<size_t>(count)));
    }
    if(!input.eof()||total!=static_cast<size_t>(info.st_size))return {};
    return hash.finish();
}
// Once per auto-load batch, never per mod/frame. Include the entire installed managed
// runtime, not just SDK timestamps: reference additions/removals and same-size edits matter.
inline std::string runtimeIdentity(const std::string& directory) {
    DIR* listing=opendir(directory.c_str());if(!listing)return {};
    std::vector<std::string> names;
    while(auto* item=readdir(listing)) {
        const std::string name=item->d_name;
        if(name.size()>4&&name.ends_with(".dll"))names.push_back(name);
    }
    closedir(listing);
    if (DIR* refs=opendir((directory+"/compiler-references").c_str())) {
        while(auto* item=readdir(refs)) {
            const std::string name=item->d_name;
            if(name.size()>4&&name.ends_with(".dll"))names.push_back("compiler-references/"+name);
        }
        closedir(refs);
    }
    std::sort(names.begin(),names.end());
    if(names.size()>512)return {};
    for(const char* required:{"BunnySourceSdk.dll","BunnySourceCompiler.dll","BunnyManagedProbe.dll",
                             "Microsoft.CodeAnalysis.dll","Microsoft.CodeAnalysis.CSharp.dll",
                             "System.Private.CoreLib.dll","System.Runtime.dll"})
        if(!std::binary_search(names.begin(),names.end(),std::string(required)))return {};
    PortableSha256 hash;hash.field(envelope);uint64_t total=0;
    for(const auto& name:names) {
        struct stat info{};const auto path=directory+"/"+name;
        if(lstat(path.c_str(),&info)!=0||info.st_size<1)return {};
        total+=static_cast<uint64_t>(info.st_size);if(total>256*1024*1024)return {};
        const auto value=fileHash(path,64*1024*1024);if(value.empty())return {};
        hash.field(name);hash.field(value);
    }
    return hash.finish();
}
inline bool validSections(const std::string& defaults,const std::string& curves,const std::string& combat) {
    return defaults.size()<=kSourceDefaultsBytes&&curves.size()<=kSourceMotionBytes&&combat.size()<=kSourceMotionBytes&&
        defaults.starts_with("BUNNY_MANAGED_DEFAULTS_V2\n")&&
        (curves.empty()||curves.starts_with("BUNNY_PROJECTILE_TURNS_V1\n"))&&
        combat.starts_with("BUNNY_COMBAT_POLICIES_V1\n");
}
inline std::string sectionsHash(const std::string& defaults,const std::string& curves,const std::string& combat) {
    PortableSha256 hash;hash.field(defaults);hash.field(curves);hash.field(combat);return hash.finish();
}
inline std::string encode(const std::string& contract,const std::string& id,const std::string& key,
                          const std::string& assembly,const std::string& defaults,
                          const std::string& curves,const std::string& combat) {
    if(!digest(key)||!digest(assembly)||!validSections(defaults,curves,combat))return {};
    return std::string(envelope)+"\n"+contract+"\n"+id+"\n"+key+"\n"+
        std::to_string(defaults.size())+" "+std::to_string(curves.size())+" "+std::to_string(combat.size())+"\n"+
        assembly+"\n"+sectionsHash(defaults,curves,combat)+"\n"+defaults+curves+combat;
}
inline bool decode(const std::string& value,const std::string& contract,const std::string& id,
                   const std::string& key,const std::string& assembly,std::string* defaults,
                   std::string* curves,std::string* combat,std::string* reason=nullptr) {
    const auto fail=[&](const char* description) {
        if(reason)*reason=description;
        return false;
    };
    if(!digest(key))return fail("InvalidSourceKey");
    if(!digest(assembly))return fail("AssemblyUnavailable");
    std::istringstream input(value);std::string line,counts,checksum;
    for(const auto& expected:{std::string(envelope),contract,id,key}) {
        if(!std::getline(input,line))return fail("HeaderTruncated");
        if(line!=expected) {
            if(expected==std::string(envelope))return fail("EnvelopeChanged");
            if(expected==contract)return fail("ContractChanged");
            if(expected==id)return fail("ModIdentityChanged");
            return fail("SourceFingerprintChanged");
        }
    }
    if(!std::getline(input,counts))return fail("CountsMissing");
    if(!std::getline(input,line))return fail("AssemblyMissing");
    if(line!=assembly)return fail("AssemblyChanged");
    if(!std::getline(input,checksum)||!digest(checksum))return fail("ChecksumMissing");
    size_t a=0,b=0,c=0;std::string extra;std::istringstream sizes(counts);
    if(!(sizes>>a>>b>>c)||(sizes>>extra)||a>kSourceDefaultsBytes||b>kSourceMotionBytes||c>kSourceMotionBytes)
        return fail("SectionSizesInvalid");
    const auto position=input.tellg();if(position<0)return fail("PayloadMissing");
    const size_t offset=static_cast<size_t>(position);
    if(value.size()!=offset+a+b+c)return fail("PayloadSizeChanged");
    const auto d=value.substr(offset,a),t=value.substr(offset+a,b),p=value.substr(offset+a+b,c);
    if(!validSections(d,t,p))return fail("SectionFormatInvalid");
    if(sectionsHash(d,t,p)!=checksum)return fail("SectionsChecksumChanged");
    *defaults=d;*curves=t;*combat=p;
    if(reason)*reason="Hit";
    return true;
}
}
