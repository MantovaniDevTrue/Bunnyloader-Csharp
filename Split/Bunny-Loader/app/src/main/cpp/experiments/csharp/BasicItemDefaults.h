#pragma once
#include <array>
#include <string>
#include <vector>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cerrno>

namespace bl::csharp {
enum BasicField { BasicWidth, BasicHeight, BasicMaxStack, BasicValue, BasicRare,
    BasicDamage, BasicCrit, BasicUseTime, BasicUseAnimation, BasicUseStyle,
    BasicPick, BasicAxe, BasicHammer, BasicFlags, BasicFieldCount };
inline constexpr std::array<const char*,BasicFieldCount> kBasicFieldNames = {
    "Width","Height","MaxStack","Value","Rare","Damage","Crit","UseTime",
    "UseAnimation","UseStyle","Pick","Axe","Hammer","Flags"};
struct BasicRecipeIngredient { std::string key; int count=1; int type=-1; };
struct BasicRecipe { int stack=1, station=-1; std::vector<BasicRecipeIngredient> ingredients; };
struct BasicItemEntry {
    std::string name; int type=-1;
    std::array<int,BasicFieldCount> values{0,0,1,0,0,0,0,0,0,0,0,0,0,0};
    float knockBack=0;
    std::string useSound,tooltip;
    std::vector<BasicRecipe> recipes;
};
inline bool basicInteger(const std::string& s,int* n) {
    if(s.empty()||s.size()>11) return false;
    auto p=std::from_chars(s.data(),s.data()+s.size(),*n);
    return p.ec==std::errc{} && p.ptr==s.data()+s.size();
}
inline std::vector<std::string> basicSplit(const std::string& s,char c) {
    std::vector<std::string> out; size_t start=0;
    for(size_t i=0;i<=s.size();++i) if(i==s.size()||s[i]==c) {out.push_back(s.substr(start,i-start));start=i+1;}
    return out;
}
inline bool validateBasicItem(const BasicItemEntry& item,std::string* error) {
    const int min[]={0,0,1,0,-1,0,-100,0,0,0,0,0,0,0};
    const int max[]={256,256,9999,100000000,11,1000,100,600,600,20,1000,200,1000,31};
    for(size_t i=0;i<BasicFieldCount;++i) if(item.values[i]<min[i]||item.values[i]>max[i]) {
        *error=item.name+": "+kBasicFieldNames[i]+" fora do limite";return false;
    }
    if(!std::isfinite(item.knockBack)||item.knockBack<0||item.knockBack>50) { *error=item.name+": KnockBack invalido";return false; }
    if(item.useSound.size()>72||item.tooltip.size()>512) { *error=item.name+": som/tooltip grande demais";return false; }
    for(unsigned char c:item.useSound) if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) { *error=item.name+": UseSound deve ser campo de SoundID";return false; }
    return true;
}
inline bool parseBasicRecipes(const std::string& text,BasicItemEntry* item,std::string* error) {
    if(text=="-")return true;
    auto recipes=basicSplit(text,';');
    if(recipes.size()>16) {*error=item->name+": mais de 16 receitas";return false;}
    for(const auto& wire:recipes) {
        auto parts=basicSplit(wire,',');BasicRecipe recipe;
        if(parts.size()<3||parts.size()>17||!basicInteger(parts[0],&recipe.stack)||
            recipe.stack<1||recipe.stack>item->values[BasicMaxStack]||!basicInteger(parts[1],&recipe.station)||
            recipe.station < -1||recipe.station>=2195) { *error=item->name+": resultado/estacao da receita invalido";return false; }
        for(size_t i=2;i<parts.size();++i) {
            auto pair=basicSplit(parts[i],':');BasicRecipeIngredient ingredient;
            if(pair.size()!=2||pair[0].size()<2||pair[0].size()>161||
                !basicInteger(pair[1],&ingredient.count)||ingredient.count<1||ingredient.count>9999) {*error=item->name+": ingrediente invalido";return false;}
            ingredient.key=pair[0];
            if(pair[0][0]=='v') {
                if(!basicInteger(pair[0].substr(1),&ingredient.type)||ingredient.type<1||ingredient.type>=6196) {*error=item->name+": ItemID vanilla invalido";return false;}
            } else if(pair[0][0]=='m') {
                for(unsigned char c:pair[0].substr(1))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/')) {*error=item->name+": chave de item invalida";return false;}
            } else {*error=item->name+": tipo de ingrediente invalido";return false;}
            recipe.ingredients.push_back(std::move(ingredient));
        }
        item->recipes.push_back(std::move(recipe));
    }
    return true;
}
inline bool parseBasicItemWire(const std::vector<std::string>& fields,BasicItemEntry* item,std::string* error) {
    if(fields.size()!=20||fields[0]!="ITEM") {*error="ITEM requer 20 campos";return false;}
    BasicItemEntry out;out.name=fields[1];
    for(size_t i=0;i<BasicFieldCount;++i)if(!basicInteger(fields[i+2],&out.values[i])) {*error=out.name+": inteiro invalido em "+kBasicFieldNames[i];return false;}
    char* end=nullptr;errno=0;out.knockBack=std::strtof(fields[16].c_str(),&end);
    if(errno||!end||*end||fields[16].empty()) {*error=out.name+": KnockBack invalido";return false;}
    out.useSound=fields[17]=="-"?"":fields[17];
    const auto& hex=fields[18];
    if(hex!="-") {
        if(hex.size()%2||hex.size()>1024) {*error=out.name+": tooltip hex invalido";return false;}
        auto digit=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
        for(size_t i=0;i<hex.size();i+=2) {int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0){*error=out.name+": tooltip hex invalido";return false;}char c=static_cast<char>((a<<4)|b);if(c==0||static_cast<unsigned char>(c)==127){*error=out.name+": tooltip com controle invalido";return false;}out.tooltip+=c;}
    }
    if(!validateBasicItem(out,error)||!parseBasicRecipes(fields[19],&out,error))return false;
    *item=std::move(out);return true;
}
}
