#include "experiments/csharp/SourceLoader.cpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
 using namespace bl::csharp;assert(argc==4);
 std::ifstream file(argv[1]);std::string source((std::istreambuf_iterator<char>(file)),{}),error;
 std::vector<ParsedSourceType> types;assert(parseCSharpSource(source,&types,&error));assert(types.size()==1);
 NativeContentSpec spec{};std::string target,display,texture;
 assert(fields(types[0],&spec,&target,&display,&texture,&error));
 assert(spec.kind==NativeProjectile&&display=="Example Minion"&&texture=="Content/Projectiles/ExampleMinion.png");
 types.clear();std::ifstream gunFile(argv[2]);source.assign((std::istreambuf_iterator<char>(gunFile)),{});
 assert(parseCSharpSource(source,&types,&error)&&types.size()==1);
 assert(fields(types[0],&spec,&target,&display,&texture,&error));
 assert(spec.kind==NativeBasicItem&&types[0].name=="ExampleGun"&&display=="Example Gun"&&texture=="Content/Items/Weapons/Ranged/ExampleGun.png");
 assert(types[0].localizedNames.size()==2&&types[0].localizedNames[1].second=="Example Arma");
 types.clear();std::ifstream shotgunFile(argv[3]);source.assign((std::istreambuf_iterator<char>(shotgunFile)),{});
 assert(parseCSharpSource(source,&types,&error)&&types.size()==1);
 assert(fields(types[0],&spec,&target,&display,&texture,&error));
 assert(spec.kind==NativeBasicItem&&display=="Example Shotgun"&&texture=="Content/Items/Weapons/Ranged/ExampleShotgun.png");
 // Runtime registration uses only names/assets; Roslyn executes arbitrary numeric/movement defaults.
 types.clear();source=R"(public sealed class IndependentMinion:BunnyProjectile {
 public override void SetDefaults(){DisplayName="Independent";Texture="independent.png";
 int count=0;for(int i=0;i<3;++i)count+=i;Width=count*6;Minion=true;MinionSlots=1;AiStyle=0;
 MinionMovement=new BunnyMinionMovement(followSpeed:4f,attackSpeed:8f);}
 })";
 assert(parseCSharpSource(source,&types,&error));assert(fields(types[0],&spec,&target,&display,&texture,&error));
 assert(types[0].properties.size()==2&&display=="Independent");
 types.clear();source=R"([BunnyName("en-US","Sword")][BunnyName("pt-BR","Espada")]
 public sealed class LocalizedSword:BunnyItem {public override void SetDefaults(){DisplayName="Sword";Texture="sword.png";}})";
 assert(parseCSharpSource(source,&types,&error)&&types[0].localizedNames.size()==2);
 assert(types[0].localizedNames[1].first=="pt-BR"&&types[0].localizedNames[1].second=="Espada");
 types.clear();source=R"([BunnyName("pt-BR","Espada")][BunnyName("pt-BR","Outra")]
 public sealed class Duplicate:BunnyItem{public override void SetDefaults(){}})";
 assert(!parseCSharpSource(source,&types,&error)&&error.find("cultura unica")!=std::string::npos);
 std::cout<<"SUMMON_PRODUCTION_SOURCE_PARSER_AND_EARLY_REGISTRATION_NAMES_ASSETS_AND_COMPUTED_DEFAULTS_PASS\n";
}
