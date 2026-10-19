#pragma once
#include "il2cpp/Types.h"
#include <string>
#include <vector>

namespace bl::csharp::shooting {
struct Entry {std::string mod,name,source,assembly;int type=-1,mask=0;};
bool publish(const std::vector<Entry>& entries,std::string* error);
bool bindAssembly(const std::string& mod,const std::string& assembly,std::string* error);
bool hasPolicies(const std::string& mod);
}
