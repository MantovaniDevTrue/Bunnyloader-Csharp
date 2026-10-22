#pragma once
#include "content/backgrounds/SurfaceGeometry.h"
#include <vector>

namespace bl::runtime::surface {
enum class Language { Managed, JavaScript };
enum class Operation { Fades, Far, Middle, Close };
struct Candidate {int slot=-1,priority=0;double weight=.5;};
struct CustomDraw {int texture=-1;DrawPlan plan;Color color;};
struct Request {
    int slot=-1;Operation operation=Operation::Far;double speed=0;
    double alpha=0;Color color;BackGeometry back;CloseGeometry close;
    std::vector<float> fades;
};
struct Reply {
    int texture=-1;bool defaultClose=true;
    CloseSettings close;
    std::vector<float> fades;
    std::vector<CustomDraw> custom;
};
using Choose=Candidate(*)();
using Menu=int(*)();
using Invoke=bool(*)(const Request&,Reply*,std::string*);
// Native ownership/selection/rendering is independent of both virtual machines.
int reserve(const std::string& mod,const std::string& name,Language language,std::string* error);
int find(const std::string& mod,const std::string& name);
int vanillaCount(std::string* error);
int count();
bool select(int slot,int priority,double weight,bool active,bool menu,std::string* error);
void provider(Language language,Choose choose,Menu menu,Invoke invoke);
void wake();
void retire(int slot);
void retireLanguage(Language language);
void resetWorld();
bool install(std::string* error);
}
