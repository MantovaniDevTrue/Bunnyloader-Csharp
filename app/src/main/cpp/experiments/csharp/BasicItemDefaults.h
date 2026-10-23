#pragma once
#include "experiments/csharp/BasicArmorPolicy.h"
#include <array>
#include <string>
#include <vector>
#include <map>
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
struct BasicRecipeIngredient { std::string key; int count=1; int type=-1;
    std::string customGroup{}, groupDescription{}; std::vector<std::string> groupMembers{};
    bool operator==(const BasicRecipeIngredient&) const = default; };
struct BasicRecipe { int stack=1, station=-1; std::vector<BasicRecipeIngredient> ingredients; std::string stationKey; int callbacks=0;
    bool operator==(const BasicRecipe&) const = default; };
struct BasicAccessoryEffect { std::string field,operation; float value=0; };
struct BasicItemEntry {
    std::string name; int type=-1; int defense=0; bool defenseSpecified=false;
    std::array<int,BasicFieldCount> values{0,0,1,0,0,0,0,0,0,0,0,0,0,0};
    float knockBack=0;
    std::string useSound,tooltip,behavior,modDir,modId,sourceFile,tooltipPlan;
    std::map<std::string,std::string> localizedTooltips;
    std::vector<BasicAccessoryEffect> accessoryEffects;
    BasicArmorPolicy armor;
    std::string placeTileKey; int placeTile=-1,placeStyle=0; bool placeable=false,wallPlaceable=false;
    int wallIntegerOverrides=8191,wallFlagOverrides=127;
    bool behaviorInstalled=false;
    std::string behaviorError;
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
inline bool validBasicAccessoryEffect(const BasicAccessoryEffect& e) {
    if(!std::isfinite(e.value))return false;
    if(e.field=="strongBees")return e.operation=="set"&&e.value==1;
    if(e.field!="meleeDamage"&&e.field!="rangedDamage"&&e.field!="magicDamage"&&e.field!="minionDamage"&&e.field!="moveSpeed")return false;
    return (e.operation=="add"||e.operation=="mul")&&e.value>=-10&&e.value<=10&&(e.operation!="mul"||e.value>=0);
}
inline bool validateBasicItem(const BasicItemEntry& item,std::string* error) {
    const int min[]={0,0,1,0,-1,0,-100,0,0,0,0,0,0,0};
    const int max[]={256,256,9999,100000000,4107,1000,100,600,600,20,1000,200,1000,127};
    for(size_t i=0;i<BasicFieldCount;++i) if(item.values[i]<min[i]||item.values[i]>max[i]) {
        *error=item.name+": "+kBasicFieldNames[i]+" fora do limite";return false;
    }
    if(!std::isfinite(item.knockBack)||item.knockBack<0||item.knockBack>50) { *error=item.name+": KnockBack invalido";return false; }
    if(item.accessoryEffects.size()>16||(!item.accessoryEffects.empty()&&!(item.values[BasicFlags]&32))) {
        *error=item.name+": politica nativa de acessorio invalida";return false;
    }
    for(const auto& e:item.accessoryEffects)if(!validBasicAccessoryEffect(e)) {
        *error=item.name+": efeito nativo de acessorio invalido";return false;
    }
    if(item.useSound.size()>72||item.tooltip.size()>512) { *error=item.name+": som/tooltip grande demais";return false; }
    for(unsigned char c:item.useSound) if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) { *error=item.name+": UseSound deve ser campo de SoundID";return false; }
    return true;
}
inline bool parseBasicRecipes(const std::string& text,BasicItemEntry* item,std::string* error) {
    if(text=="-")return true;
    auto recipes=basicSplit(text,';');
    if(recipes.size()>16) {*error=item->name+": mais de 16 receitas";return false;}
    for(const auto& wire:recipes) {
        auto policy=basicSplit(wire,'@');BasicRecipe recipe;
        if(policy.size()>2||(policy.size()==2&&(!basicInteger(policy[1],&recipe.callbacks)||recipe.callbacks<1||recipe.callbacks>7))) {
            *error=item->name+": Stage=ParseRecipeCallbacks Expected=Mask1..7 Reason=InvalidCallbackDescriptor";return false;
        }
        auto parts=basicSplit(policy[0],',');
        if(parts.size()<3||parts.size()>17||!basicInteger(parts[0],&recipe.stack)||
            recipe.stack<1||recipe.stack>item->values[BasicMaxStack]) { *error=item->name+": resultado da receita invalido";return false; }
        if(parts[1].size()>1&&parts[1][0]=='m') {
            const auto key=parts[1].substr(1);
            bool valid=key.size()<=160&&key.front()!='/'&&key.back()!='/';
            unsigned slashes=0;
            for(unsigned char c:key) {
                if(c=='/') {if(++slashes>1)valid=false;continue;}
                if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))valid=false;
            }
            if(!valid){*error=item->name+": Stage=ParseRecipe Field=station Expected=ModTileKey Actual="+parts[1]+" Reason=InvalidStationKey";return false;}
            recipe.stationKey=key;
        } else if(!basicInteger(parts[1],&recipe.station)||recipe.station < -1||recipe.station>=754) {
            *error=item->name+": Stage=ParseRecipe Field=station Expected=VanillaTileIdOrModTileKey Actual="+parts[1]+" Reason=InvalidStation";return false;
        }
        for(size_t i=2;i<parts.size();++i) {
            auto pair=basicSplit(parts[i],':');BasicRecipeIngredient ingredient;
            if(pair.size()!=2||pair[0].size()<2||pair[0].size()>48000||
                !basicInteger(pair[1],&ingredient.count)||ingredient.count<1||ingredient.count>9999) {
                *error="[CSharp][ERROR][Recipe] Class="+item->name+" API=BunnyRecipe Stage=ParseIngredient Expected=TypedKey:Count(1..9999) Actual="+parts[i]+" Result=Rejected Reason=InvalidIngredientDescriptor";return false;
            }
            ingredient.key=pair[0];
                    if(pair[0][0]=='v') {
                if(!basicInteger(pair[0].substr(1),&ingredient.type)||ingredient.type<1||ingredient.type>=6196) {*error=item->name+": ItemID vanilla invalido";return false;}
            } else if(pair[0][0]=='m') {
                for(unsigned char c:pair[0].substr(1))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/')) {*error=item->name+": chave de item invalida";return false;}
            } else if(pair[0][0]=='h') {
                const auto fields=basicSplit(pair[0].substr(1),'~');
                if(fields.size()!=3||fields[0].empty()||fields[0].size()>72||fields[1].empty()||fields[1].size()>512||fields[1].size()%2) {
                    *error=item->name+": Stage=ParseRecipeGroup Reason=InvalidCustomGroupDescriptor";return false;
                }
                for(unsigned char c:fields[0])if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) {
                    *error=item->name+": Stage=ParseRecipeGroup Reason=InvalidGroupName";return false;
                }
                auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
                for(size_t n=0;n<fields[1].size();n+=2){const int a=nibble(fields[1][n]),b=nibble(fields[1][n+1]);
                    if(a<0||b<0||((a<<4)|b)<32||((a<<4)|b)==127){*error=item->name+": Stage=ParseRecipeGroup Reason=InvalidDescriptionHex";return false;}
                    ingredient.groupDescription+=static_cast<char>((a<<4)|b);
                }
                ingredient.customGroup=fields[0];ingredient.groupMembers=basicSplit(fields[2],'+');
                if(ingredient.groupMembers.empty()||ingredient.groupMembers.size()>256){*error=item->name+": Stage=ParseRecipeGroup Reason=MemberLimit";return false;}
                for(const auto& member:ingredient.groupMembers){
                    if(member.size()<2||member.size()>161||(member[0]!='v'&&member[0]!='m')){*error=item->name+": Stage=ParseRecipeGroup Reason=InvalidMember";return false;}
                    for(unsigned char c:member.substr(1))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/')){*error=item->name+": Stage=ParseRecipeGroup Reason=InvalidMemberKey";return false;}
                }
            } else if(pair[0][0]=='g') {
                const auto name=pair[0].substr(1);
                bool valid=name.size()<=72&&((name.front()>='A'&&name.front()<='Z')||(name.front()>='a'&&name.front()<='z'));
                for(unsigned char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))valid=false;
                if(!valid){*error=item->name+": Stage=ParseRecipe Field=group Expected=RecipeGroupsField Actual="+name+" Reason=InvalidGroupField";return false;}
            } else {*error=item->name+": tipo de ingrediente invalido";return false;}
            recipe.ingredients.push_back(std::move(ingredient));
        }
        item->recipes.push_back(std::move(recipe));
    }
    return true;
}
inline bool parseBasicAccessoryEffects(const std::string& wire,BasicItemEntry* item,std::string* error) {
    if(wire=="-")return true;
    if(wire.empty()||wire.size()>2048){*error=item->name+": descricao de efeitos nativos invalida";return false;}
    const auto operations=basicSplit(wire,';');
    if(operations.size()>16){*error=item->name+": efeitos nativos excedem 16";return false;}
    for(const auto& token:operations) {
        auto parts=basicSplit(token,':');
        if(parts.size()!=3){*error=item->name+": efeito nativo incompleto";return false;}
        const auto& field=parts[0];const auto& operation=parts[1];
        if(field=="strongBees") {
            if(operation!="set"||parts[2]!="1"){*error=item->name+": flag nativa invalida";return false;}
            item->accessoryEffects.push_back({field,operation,1.f});
            continue;
        }
        if(field!="meleeDamage"&&field!="rangedDamage"&&field!="magicDamage"&&field!="minionDamage"&&field!="moveSpeed") {
            *error=item->name+": atributo nativo invalido";return false;
        }
        if(operation!="add"&&operation!="mul"){*error=item->name+": operacao nativa invalida";return false;}
        if(parts[2].empty()||parts[2].find_first_not_of("0123456789.eE+-")!=std::string::npos) {
            *error=item->name+": formato de valor nativo invalido";return false;
        }
        char* end=nullptr;errno=0;const float value=std::strtof(parts[2].c_str(),&end);
        if(errno||parts[2].empty()||!end||*end||!std::isfinite(value)||value<-10.f||value>10.f||
            (operation=="mul"&&value<0.f)){*error=item->name+": valor de efeito nativo invalido";return false;}
        item->accessoryEffects.push_back({field,operation,value});
    }
    if(!(item->values[BasicFlags]&32)){*error=item->name+": efeitos nativos exigem Accessory=true";return false;}
    return true;
}
inline bool parseBasicItemWire(const std::vector<std::string>& fields,BasicItemEntry* item,std::string* error) {
    if((fields.size()<20||fields.size()>27)||fields[0]!="ITEM") {*error="ITEM requer 20 a 27 campos";return false;}
    BasicItemEntry out;out.name=fields[1];
    for(size_t i=0;i<BasicFieldCount;++i)if(!basicInteger(fields[i+2],&out.values[i])) {*error=out.name+": inteiro invalido em "+kBasicFieldNames[i];return false;}
    if (out.values[BasicRare] > 11) {
        *error = out.name + ": Rare nativo de origem precisa ser vanilla; use SetRarity<T>()";
        return false;
    }
    char* end=nullptr;errno=0;out.knockBack=std::strtof(fields[16].c_str(),&end);
    if(errno||!end||*end||fields[16].empty()) {*error=out.name+": KnockBack invalido";return false;}
    out.useSound=fields[17]=="-"?"":fields[17];
    const auto& hex=fields[18];
    if(hex!="-") {
        if(hex.size()%2||hex.size()>1024) {*error=out.name+": tooltip hex invalido";return false;}
        auto digit=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
        for(size_t i=0;i<hex.size();i+=2) {int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0){*error=out.name+": tooltip hex invalido";return false;}char c=static_cast<char>((a<<4)|b);if(c==0||static_cast<unsigned char>(c)==127){*error=out.name+": tooltip com controle invalido";return false;}out.tooltip+=c;}
    }
    if(fields.size()>=21&&fields[20]!="-") {
        const auto& wire=fields[20];
        if(wire.empty()||wire.size()>8192||wire.size()%2) {*error=out.name+": comportamento hex invalido";return false;}
        auto digit=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
        for(size_t i=0;i<wire.size();i+=2) {
            const int a=digit(wire[i]),b=digit(wire[i+1]);
            if(a<0||b<0||((a<<4)|b)<32) {*error=out.name+": comportamento hex invalido";return false;}
            out.behavior+=static_cast<char>((a<<4)|b);
        }
    }
    if(fields.size()>=22 && fields[21]!="-") {
        auto pair=basicSplit(fields[21],':');
        if((pair.size()!=2&&pair.size()!=4)||pair[0].size()<2||!basicInteger(pair[1],&out.placeStyle)||out.placeStyle<0||out.placeStyle>32767) {
            *error=out.name+": Stage=ParsePlacement Field=placeStyle Expected=0..32767 Actual="+fields[21]+" Reason=InvalidPlacement";return false;
        }
        if(pair[0].starts_with("w")) {
            out.wallPlaceable=true;pair[0].erase(0,1);
            if(pair[0].size()<2||out.placeStyle!=0){*error=out.name+": Stage=ParsePlacement Reason=InvalidWallTokenOrStyle";return false;}
        }
        if(pair.size()==4) {
            if(!out.wallPlaceable||!basicInteger(pair[2],&out.wallIntegerOverrides)||out.wallIntegerOverrides<0||out.wallIntegerOverrides>8191||
               !basicInteger(pair[3],&out.wallFlagOverrides)||out.wallFlagOverrides<0||out.wallFlagOverrides>127) {
                *error=out.name+": Stage=ParsePlacement Reason=InvalidWallOverrideMask Expected=Integers0..8191:Flags0..127 Actual="+fields[21];return false;
            }
        }
        if(pair[0][0]=='v') {
            if(!basicInteger(pair[0].substr(1),&out.placeTile)||out.placeTile<(out.wallPlaceable?1:0)||out.placeTile>=(out.wallPlaceable?367:754)) {
                *error=out.name+": Stage=ParsePlacement Field=createTile Expected=VanillaTileId Actual="+pair[0]+" Reason=InvalidTileId";return false;
            }
        } else if(pair[0][0]=='m') {
            out.placeTileKey=pair[0].substr(1);const auto& key=out.placeTileKey;
            bool valid=!key.empty()&&key.size()<=160&&key.front()!='/'&&key.back()!='/';unsigned slashes=0;
            for(size_t i=0;i<key.size();++i){unsigned char c=key[i];if(c=='/') {if(++slashes>1||i==0||key[i-1]=='/')valid=false;}
                else if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))valid=false;}
            if(!valid){*error=out.name+": Stage=ParsePlacement Field=createTile Expected=NameOrUID/Name Actual="+key+" Reason=InvalidContentKey";return false;}
        } else {*error=out.name+": Stage=ParsePlacement Reason=UnknownTileToken Actual="+pair[0];return false;}
        out.placeable=true;
    }
    if(fields.size()>=23 && fields[22]!="-") {
        if(fields[22].empty()||fields[22]=="-"||fields[22].size()>9000) {
            *error=out.name+": traducoes de tooltip vazias ou grandes demais";return false;
        }
        const auto values=basicSplit(fields[22],';');
        if(values.empty()||values.size()>8) {
            *error=out.name+": quantidade de traducoes invalida";return false;
        }
        auto digit=[](char c)->int {
            return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;
        };
        for(const auto& value:values) {
            const size_t colon=value.find(':');
            if(colon==std::string::npos) {*error=out.name+": traducao sem cultura";return false;}
            const std::string culture=value.substr(0,colon);
            const std::string hex=value.substr(colon+1);
            if(culture.size()<2||culture.size()>20||hex.empty()||hex.size()%2||hex.size()>1024) {
                *error=out.name+": traducao de tooltip malformada";return false;
            }
            for(unsigned char c:culture)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                (c>='0'&&c<='9')||c=='-')) {
                *error=out.name+": cultura invalida";return false;
            }
            std::string decoded;decoded.reserve(hex.size()/2);
            for(size_t i=0;i<hex.size();i+=2) {
                const int a=digit(hex[i]),b=digit(hex[i+1]);
                if(a<0||b<0||((a<<4)|b)==0||((a<<4)|b)==127) {
                    *error=out.name+": traducao hex invalida";return false;
                }
                decoded.push_back(static_cast<char>((a<<4)|b));
            }
            if(!out.localizedTooltips.emplace(culture,std::move(decoded)).second) {
                *error=out.name+": cultura duplicada";return false;
            }
        }
    }
    // A missing field or '-' inherits vanilla defaults. The numeric value 0 is
    // an explicit override, required when hot reload removes an old defense bonus.
    if(fields.size()>=24 && fields[23]!="-") {
        if(!basicInteger(fields[23],&out.defense)||out.defense<0||out.defense>1000) {
            *error=out.name+": defesa nativa fora do limite";return false;
        }
        out.defenseSpecified=true;
    }
    if(fields.size()>=25&&!parseBasicAccessoryEffects(fields[24],&out,error))return false;
    if(fields.size()>=26&&fields[25]!="-"&&!parseBasicArmorPolicy(fields[25],&out.armor)) {
        *error=out.name+": politica nativa de armadura invalida";return false;
    }
    if(fields.size()==27&&fields[26]!="-") {
        const std::string& hex=fields[26];
        if(hex.size()>8192||hex.empty()||hex.size()%2) {
            *error=out.name+": tooltip draw plan oversized";return false;
        }
        auto nibble=[](char value)->int{
            return value>='0'&&value<='9'?value-'0':
                value>='A'&&value<='F'?value-'A'+10:
                value>='a'&&value<='f'?value-'a'+10:-1;
        };
        for(size_t i=0;i<hex.size();i+=2) {
            const int a=nibble(hex[i]),b=nibble(hex[i+1]);
            if(a<0||b<0||((a<<4)|b)<32) {
                *error=out.name+": invalid tooltip plan hex";return false;
            }
            out.tooltipPlan.push_back(static_cast<char>((a<<4)|b));
        }
        if(out.tooltipPlan.size()<2||out.tooltipPlan.front()!='['||
           out.tooltipPlan.back()!=']') {
            *error=out.name+": invalid tooltip plan envelope";return false;
        }
    }
    if(!validateBasicItem(out,error)||!parseBasicRecipes(fields[19],&out,error))return false;
    *item=std::move(out);return true;
}
}
