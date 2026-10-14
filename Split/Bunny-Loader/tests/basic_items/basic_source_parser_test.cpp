#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<bl::csharp::ParsedSourceType> types;std::string error;
 const std::string source=R"(public sealed class Tool : BunnyItem {
 public override void SetDefaults() {
  DisplayName="Tool"; Texture="Content/Items/Tool.png";
  int baseDamage=20; Damage=baseDamage+5; if(Damage>20) AutoReuse=true;
 }
 public override void AddRecipes(){ CreateRecipe().AddIngredient(2,10).AddTile(18).Register(); }
 })";
 assert(bl::csharp::parseCSharpSource(source,&types,&error));assert(types.size()==1);
 assert(types[0].base=="BunnyItem"&&types[0].properties.size()==2);
 assert(types[0].properties.at("Texture")=="Content/Items/Tool.png");
 types.clear();assert(!bl::csharp::parseCSharpSource("public class Tool : BunnyItem {public override void SetDefaults(){Texture=MakePath();}}",&types,&error));
 assert(error.find("Texture precisa de string literal")!=std::string::npos);
 std::cout<<"BASIC_ITEM_RESOURCE_PARSER_PASS=YES\n";
}
