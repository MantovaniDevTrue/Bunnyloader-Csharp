// Teste de host (sem Android) da politica do buff de minion e do parser das
// novas propriedades de BunnyBuff (MinionProjectile, NoSave, NoTimeDisplay).
// Nao valida o comportamento no Terraria Android.
#include "experiments/csharp/ManagedMinionBuff.h"
#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <iostream>
using namespace bl::csharp;
int main(){
  assert(decideMinionBuff(-1,5)==MinionBuffAction::Keep);          // sem leitura
  assert(decideMinionBuff(0,18000)==MinionBuffAction::Remove);     // sem minion
  assert(decideMinionBuff(0,2)==MinionBuffAction::Remove);
  assert(decideMinionBuff(1,2)==MinionBuffAction::Refresh);        // acabou de invocar
  assert(decideMinionBuff(3,17999)==MinionBuffAction::Refresh);
  assert(decideMinionBuff(2,kMinionBuffTicks)==MinionBuffAction::Keep);
  std::string src=R"(using BunnyLoader.Source;
namespace M; public sealed class ExampleMinionBuff : BunnyBuff {
 public override void SetDefaults(){ DisplayName="Example Minion"; Texture="Content/Buffs/ExampleMinionBuff.png";
 NoSave = true; NoTimeDisplay = true; MinionProjectile = nameof(ExampleMinion); } })";
  std::vector<ParsedSourceType> d; std::string err;
  assert(parseCSharpSource(src,&d,&err,false));
  assert(d.size()==1 && d[0].properties.at("MinionProjectile")=="ExampleMinion" && d[0].properties.at("NoSave")=="true");
  int32_t types[]={401,8,402,403,0},times[]={10,20,30,40,0};
  std::vector<int> visited;
  visitManagedBuffSlots(types,times,5,[&](size_t slot){
    visited.push_back(types[slot]);
    if(types[slot]==402){for(size_t next=slot+1;next<5;++next){types[next-1]=types[next];times[next-1]=times[next];}types[4]=0;times[4]=0;}
  });
  assert((visited==std::vector<int>{403,402,8,401}));
  assert(types[2]==403&&types[3]==0);
  std::cout<<"MINION_BUFF_POLICY_AND_ALL_SLOTS_REVERSE_REMOVAL_PASS\n";
}
