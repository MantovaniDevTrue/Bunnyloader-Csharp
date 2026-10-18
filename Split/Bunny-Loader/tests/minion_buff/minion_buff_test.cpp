// Teste de host (sem Android) da politica do buff de minion e do parser das
// novas propriedades de BunnyBuff (MinionProjectile, NoSave, NoTimeDisplay).
//
//   g++ -std=c++17 -I Split/Bunny-Loader/app/src/main/cpp \
//       Split/Bunny-Loader/tests/minion_buff/minion_buff_test.cpp \
//       Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/SourceParser.cpp \
//       -o /tmp/minion_buff_test && /tmp/minion_buff_test
//
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
  std::cout<<"ok\n";
}
