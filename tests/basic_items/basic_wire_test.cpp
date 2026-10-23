#include "experiments/csharp/BasicItemDefaults.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace bl::csharp;
int main(int argc,char** argv){
 assert(argc==2);std::ifstream file(argv[1]);std::string line,error;int count=0;
 assert(std::getline(file,line)&&line=="BUNNY_MANAGED_DEFAULTS_V2");
 while(std::getline(file,line)) {
  if(line.empty())continue;
  auto fields=basicSplit(line,'\t');BasicItemEntry entry;
  assert(parseBasicItemWire(fields,&entry,&error));assert(entry.recipes.size()==1);
  if(entry.name=="ExamplePickaxe"){assert(entry.values[BasicPick]==220&&entry.values[BasicDamage]==20);assert(entry.values[BasicValue]==50000);assert(entry.useSound=="Item1");}
  else if(entry.name=="ExampleHamaxe"){assert(entry.values[BasicAxe]==30&&entry.values[BasicHammer]==100&&entry.values[BasicDamage]==25);}
  else if(entry.name=="ExampleItem"){assert(entry.values[BasicMaxStack]==9999&&entry.recipes[0].stack==999&&entry.recipes[0].ingredients[0].type==2);
    assert(entry.tooltip=="Item description"&&entry.localizedTooltips.at("pt-BR")=="Descrição do item");}
  else assert(false);
  auto bad=fields;bad[2]="99999";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[16]="NaN";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[17]="Item1;inject";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[19]="1,18,v0:10";assert(!parseBasicItemWire(bad,&entry,&error));
  auto station=fields;station[19]="1,mExampleWorkbench,v2:1";
  assert(parseBasicItemWire(station,&entry,&error));assert(entry.recipes[0].station==-1&&entry.recipes[0].stationKey=="ExampleWorkbench");
  for(const std::string key:{"m../bad","m/bad","mmod//bad","mmod/a/b","754","2195","m"}) {
   station[19]="1,"+key+",v2:1";assert(!parseBasicItemWire(station,&entry,&error));
  }
  auto placement=fields;
  if(placement.size()==23)placement.resize(22);
  if(placement.size()==20)placement.push_back("-");
  if(placement.size()==21)placement.push_back("mmod/Bench:3");
  else placement[21]="mmod/Bench:3";
  assert(parseBasicItemWire(placement,&entry,&error)&&entry.placeable&&entry.placeTileKey=="mod/Bench"&&entry.placeStyle==3);
  placement[21]="v18:0";assert(parseBasicItemWire(placement,&entry,&error)&&entry.placeTile==18);
  for(const std::string token:{"m../bad:0","mmod//Tile:0","mmod/a/b:0","m:0","v754:0","v18:-1","v18:32768","v18:0:1"}) {
      placement[21]=token;assert(!parseBasicItemWire(placement,&entry,&error));assert(error.find("Stage=ParsePlacement")!=std::string::npos);
  }
  if(fields.size()==23) {
    auto broken=fields;broken[22]="pt-BR:GG";assert(!parseBasicItemWire(broken,&entry,&error));
    broken[22]=fields[22]+";"+fields[22];assert(!parseBasicItemWire(broken,&entry,&error));
  }
  // Reusing the output must also reset the presence flag after an omitted field.
  auto defense=fields;defense.resize(24,"-");defense[23]="0";
  assert(parseBasicItemWire(defense,&entry,&error)&&entry.defenseSpecified&&entry.defense==0);
  defense[23]="1000";
  assert(parseBasicItemWire(defense,&entry,&error)&&entry.defenseSpecified&&entry.defense==1000);
  for(const std::string value:{"-1","1001","0.5","zero"}) {
   defense[23]=value;assert(!parseBasicItemWire(defense,&entry,&error));
  }
  defense[23]="-";
  assert(parseBasicItemWire(defense,&entry,&error)&&!entry.defenseSpecified);
  assert(parseBasicItemWire(fields,&entry,&error)&&!entry.defenseSpecified);
  auto clone=fields;clone.resize(28,"-");clone[27]="C1|8191|127|1|1";
  assert(parseBasicItemWire(clone,&entry,&error)&&entry.cloneOverridesSpecified&&entry.cloneIntegerOverrides==8191&&entry.cloneFlagOverrides==127&&entry.cloneUseSoundOverride&&entry.cloneKnockBackOverride);
  clone[27]="C1|0|0|0|0";
  assert(parseBasicItemWire(clone,&entry,&error)&&entry.cloneOverridesSpecified&&!entry.cloneUseSoundOverride&&!entry.cloneKnockBackOverride);
  for(const std::string mask:{"C2|0|0|0|0","C1|8192|0|0|0","C1|-1|0|0|0","C1|0|128|0|0","C1|0|0|2|0","C1|0|0|0|2","C1|0|0|0","C1|0|0|0|0|0"}) {
   clone[27]=mask;assert(!parseBasicItemWire(clone,&entry,&error));
  }
  assert(parseBasicItemWire(fields,&entry,&error)&&!entry.cloneOverridesSpecified);
  auto native=fields;
  while(native.size()<24)native.push_back("-");
  native[15]="32";
  native.push_back("meleeDamage:mul:1.1;strongBees:set:1;moveSpeed:add:0.05");
  assert(parseBasicItemWire(native,&entry,&error)&&entry.accessoryEffects.size()==3);
  auto invalid=native;invalid[24]="moveSpeed:mul:-1";
  assert(!parseBasicItemWire(invalid,&entry,&error));
  invalid[24]="strongBees:set:0";assert(!parseBasicItemWire(invalid,&entry,&error));
  invalid[24]="unsafeField:add:1";assert(!parseBasicItemWire(invalid,&entry,&error));
  invalid=native;invalid[15]="0";assert(!parseBasicItemWire(invalid,&entry,&error));
  ++count;
 }
 assert(count==3);std::cout<<"BASIC_ITEM_CSHARP_WIRE_PASS=YES\n";
}
