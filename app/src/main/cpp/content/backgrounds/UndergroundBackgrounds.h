#pragma once
#include <array>
#include <string>
#include <cstdint>
namespace bl::runtime::underground {
enum class Language { Managed, JavaScript };
using Slots=std::array<int32_t,5>;
struct Candidate { int slot=-1,priority=0;double weight=.5; };
using Choose=Candidate(*)();
using Fill=bool(*)(int,Slots*,std::string*);
// Registration/selection are pure native state and may be used by either VM.
int reserve(const std::string& mod,const std::string& name,Language language,std::string* error);
int find(const std::string& mod,const std::string& name);
bool select(int slot,int priority,double weight,bool active,std::string* error);
void retire(int slot);
void retireLanguage(Language language);
int count();
void provider(Language language,Choose choose,Fill fill);
bool install(std::string* error);
void resetWorld();
}
