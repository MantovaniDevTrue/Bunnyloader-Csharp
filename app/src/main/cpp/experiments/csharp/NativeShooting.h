#pragma once
#include "il2cpp/Types.h"
#include <string>
#include <vector>

namespace bl::csharp::shooting {
struct Entry {std::string mod,name,source,assembly;int type=-1,mask=0;bool cursor=false;};
bool publish(const std::vector<Entry>& entries,std::string* error);
bool bindAssembly(const std::string& mod,const std::string& assembly,std::string* error);
void unload(const std::string& mod);
// BunnyGlobalItem em itens vanilla (source == kGlobalItemSource). Substitui as
// entradas de GlobalItem do mod; tipos que já têm dono de outro mod ficam com
// ele (um dono por tipo). Enfileira o warmup de cada dono.
inline constexpr const char* kGlobalItemSource = "GlobalItem";
bool publishGlobal(const std::string& mod,const std::vector<Entry>& entries,std::string* error,int* skipped=nullptr);
// Only managed callbacks require Mono warmup; mask zero is native-only aiming.
bool hasPolicies(const std::string& mod);
}
