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
  else if(entry.name=="ExampleItem"){assert(entry.values[BasicMaxStack]==9999&&entry.recipes[0].stack==999&&entry.recipes[0].ingredients[0].type==2);}
  else assert(false);
  auto bad=fields;bad[2]="99999";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[16]="NaN";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[17]="Item1;inject";assert(!parseBasicItemWire(bad,&entry,&error));
  bad=fields;bad[19]="1,18,v0:10";assert(!parseBasicItemWire(bad,&entry,&error));
  ++count;
 }
 assert(count==3);std::cout<<"BASIC_ITEM_CSHARP_WIRE_PASS=YES\n";
}
