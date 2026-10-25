#pragma once
#include "experiments/csharp/BasicItemDefaults.h"
#include <set>

namespace bl::csharp {
struct BasicEquipTexture { std::string kind,path; int flags=0; };
struct BasicCostumePolicy {
    bool enabled=false;
    std::string alternate,buff;
    float jump=0;
    int fall=0,town=1;
};
struct BasicItemPolicy {
    std::string clone;
    std::vector<BasicEquipTexture> equips;
    BasicCostumePolicy costume;
    bool hair=false,robe=false,animated=false,lit=false,colored=false,wing=false;
    int frames=0,ticks=0; std::array<bool,3> animationFlags{};
    std::array<float,3> light{}; bool essence=false;
    std::array<uint8_t,4> color{};
    std::array<float,8> wings{};
};
namespace item_policy_wire {
// Bounded data parser, independent of any scripting engine. The SDK writes JSON
// as a serialization format; these descriptors never contain executable code.
struct Json {
    enum Kind { Null,Boolean,Number,String,Array,Object } kind=Null;
    bool boolean=false; float number=0; std::string string;
    std::vector<Json> array; std::map<std::string,Json> object;
    const Json& at(const std::string& key) const {
        static const Json missing;const auto it=object.find(key);return it==object.end()?missing:it->second;
    }
    bool integer(int lo,int hi) const {return kind==Number&&std::floor(number)==number&&number>=lo&&number<=hi;}
};
struct Parser {
    const std::string& text; size_t pos=0,nodes=0;
    void spaces(){while(pos<text.size()&&(text[pos]==' '||text[pos]=='\t'||text[pos]=='\r'||text[pos]=='\n'))++pos;}
    bool take(char c){spaces();if(pos<text.size()&&text[pos]==c){++pos;return true;}return false;}
    bool quoted(std::string* out) {
        if(!take('"'))return false;
        while(pos<text.size()) {
            auto c=static_cast<unsigned char>(text[pos++]);if(c=='"')return true;
            if(c<32)return false;
            if(c=='\\') {
                if(pos==text.size())return false;
                c=static_cast<unsigned char>(text[pos++]);
                if(c!='"'&&c!='\\'&&c!='/')return false;
            }
            out->push_back(static_cast<char>(c));if(out->size()>2048)return false;
        }
        return false;
    }
    bool value(Json* out,unsigned depth=0) {
        if(depth>8||++nodes>512)return false;
        spaces();if(pos==text.size())return false;
        if(text[pos]=='"'){out->kind=Json::String;return quoted(&out->string);}
        if(take('{')) {
            out->kind=Json::Object;if(take('}'))return true;
            do {std::string key;Json v;if(!quoted(&key)||!take(':')||!value(&v,depth+1)||!out->object.emplace(key,std::move(v)).second)return false;}while(take(','));
            return take('}');
        }
        if(take('[')) {
            out->kind=Json::Array;if(take(']'))return true;
            do {Json v;if(!value(&v,depth+1))return false;out->array.push_back(std::move(v));}while(take(','));
            return take(']');
        }
        for(const auto& token:{std::string("null"),std::string("true"),std::string("false")}) {
            if(text.compare(pos,token.size(),token)==0){pos+=token.size();out->kind=token=="null"?Json::Null:Json::Boolean;out->boolean=token=="true";return true;}
        }
        const auto start=pos;
        if(pos<text.size()&&text[pos]=='-')++pos;
        if(pos==text.size()||text[pos]<'0'||text[pos]>'9')return false;
        if(text[pos]=='0')++pos;else while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;
        if(pos<text.size()&&text[pos]=='.') {
            ++pos;const auto digits=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;if(digits==pos)return false;
        }
        if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')) {
            ++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;
            const auto digits=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;if(digits==pos)return false;
        }
        const auto token=text.substr(start,pos-start);char* end=nullptr;errno=0;out->number=std::strtof(token.c_str(),&end);
        out->kind=Json::Number;return !errno&&end&&!*end&&std::isfinite(out->number);
    }
};
inline bool numbers(const Json& value,size_t count,float min,float max) {
    if(value.kind!=Json::Array||value.array.size()!=count)return false;
    for(const auto& n:value.array)if(n.kind!=Json::Number||n.number<min||n.number>max)return false;
    return true;
}
inline bool path(const std::string& p) {
    if(p.empty()||p.size()>200||p.front()=='/'||p.back()=='/'||p.find("//")!=std::string::npos)return false;
    for(unsigned char c:p)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'))return false;
    return true;
}
inline bool read(const BasicItemEntry& entry,BasicItemPolicy* out,std::string* error) {
    if(entry.behavior.empty())return true;
    Json root;Parser parser{entry.behavior};
    auto fail=[&](const char* reason){*error="[CSharp][ERROR][ItemPolicy] Class="+entry.name+" Stage=Decode Reason="+reason;return false;};
    if(entry.behavior.size()>4096||!parser.value(&root)||root.kind!=Json::Object)return fail("MalformedDescriptor");
    parser.spaces();if(parser.pos!=entry.behavior.size()||!root.at("v").integer(1,1))return fail("UnsupportedVersion");
    const std::set<std::string> fields={"class","v","clone","equips","effects","nativeAccessory","nativeArmor","animation","light","alpha","hair","robe","defense","armor","armorSet","wings","costume"};
    for(const auto& [key,value]:root.object){(void)value;if(!fields.contains(key))return fail("UnknownField");}
    for(const char* flag:{"hair","robe"})if(root.at(flag).kind!=Json::Boolean)return fail("InvalidFlag");
    out->hair=root.at("hair").boolean;out->robe=root.at("robe").boolean;
    const auto& clone=root.at("clone");if(clone.kind!=Json::String||clone.string.size()>72)return fail("InvalidClone");
    for(unsigned char c:clone.string)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return fail("InvalidClone");
    out->clone=clone.string;
    const auto& equips=root.at("equips");if(equips.kind!=Json::Array||equips.array.size()>4)return fail("InvalidEquipment");
    std::set<std::string> seen;
    for(const auto& e:equips.array) {
        if(e.kind!=Json::Array||e.array.size()!=3||e.array[0].kind!=Json::String||e.array[1].kind!=Json::String||!e.array[2].integer(0,3))return fail("InvalidEquipment");
        const auto kind=e.array[0].string;
        if((kind!="Head"&&kind!="Body"&&kind!="Legs"&&kind!="Beard"&&kind!="Back"&&kind!="Wings")||!seen.insert(kind).second||!path(e.array[1].string))return fail("InvalidEquipment");
        out->equips.push_back({kind,e.array[1].string,static_cast<int>(e.array[2].number)});
    }
    if(out->robe&&(!seen.contains("Body")||!seen.contains("Legs")))return fail("RobeRequiresBodyAndLegs");
    const auto& costume=root.at("costume");
    if(costume.kind!=Json::Null) {
        if(costume.kind!=Json::Object||costume.object.size()!=5||
           !(entry.values[BasicFlags]&32)||
           !seen.contains("Head")||!seen.contains("Body")||!seen.contains("Legs"))
            return fail("InvalidCostume");
        for(const auto& [key,value]:costume.object) {
            (void)value;
            if(key!="alt"&&key!="buff"&&key!="jump"&&key!="fall"&&key!="town")
                return fail("UnknownCostumeField");
        }
        const auto& alternate=costume.at("alt"),buff=costume.at("buff"),jump=costume.at("jump");
        if(alternate.kind!=Json::String||!path(alternate.string)||alternate.string.size()>180||
           buff.kind!=Json::String||buff.string.empty()||buff.string.size()>72||
           jump.kind!=Json::Number||jump.number<0||jump.number>20||
           !costume.at("fall").integer(0,300)||!costume.at("town").integer(1,20))
            return fail("InvalidCostume");
        for(unsigned char ch:buff.string)
            if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||
                 (ch>='0'&&ch<='9')||ch=='_'))
                return fail("InvalidCostumeBuff");
        out->costume={true,alternate.string,buff.string,jump.number,
            static_cast<int>(costume.at("fall").number),
            static_cast<int>(costume.at("town").number)};
    }
    for(const char* marker:{"nativeAccessory","nativeArmor"})if(root.at(marker).kind!=Json::Null&&root.at(marker).kind!=Json::Boolean)return fail("InvalidNativeMarker");
    if(root.at("defense").kind!=Json::Null&&!root.at("defense").integer(0,1000))return fail("InvalidDefense");
    const auto& anim=root.at("animation");
    if(anim.kind!=Json::Null) {
        if(anim.kind!=Json::Array||anim.array.size()!=5||!anim.array[0].integer(1,128)||!anim.array[1].integer(1,600))return fail("InvalidAnimation");
        for(size_t i=0;i<3;++i){if(anim.array[i+2].kind!=Json::Boolean)return fail("InvalidAnimation");out->animationFlags[i]=anim.array[i+2].boolean;}
        out->animated=true;out->frames=static_cast<int>(anim.array[0].number);out->ticks=static_cast<int>(anim.array[1].number);
    }
    const auto& light=root.at("light");
    if(light.kind!=Json::Null) {
        if(light.kind!=Json::Array||light.array.size()!=4||light.array[3].kind!=Json::Boolean)return fail("InvalidLight");
        for(size_t i=0;i<3;++i){if(light.array[i].kind!=Json::Number||light.array[i].number<0||light.array[i].number>2)return fail("InvalidLight");out->light[i]=light.array[i].number;}
        out->lit=true;out->essence=light.array[3].boolean;
    }
    const auto& alpha=root.at("alpha");
    if(alpha.kind!=Json::Null) {
        if(alpha.kind!=Json::Array||alpha.array.size()!=4)return fail("InvalidAlpha");
        for(size_t i=0;i<4;++i){if(!alpha.array[i].integer(0,255))return fail("InvalidAlpha");out->color[i]=static_cast<uint8_t>(alpha.array[i].number);}out->colored=true;
    }
    const auto& wing=root.at("wings");
    if(wing.kind!=Json::Null) {
        const auto& stats=wing.at("stats"),vertical=wing.at("vertical");
        if(!(entry.values[BasicFlags]&32)||!seen.contains("Wings")||stats.kind!=Json::Array||stats.array.size()!=3||!stats.array[0].integer(1,6000)||
            stats.array[1].kind!=Json::Number||stats.array[1].number<0||stats.array[1].number>100||stats.array[2].kind!=Json::Number||stats.array[2].number<0||stats.array[2].number>100||!numbers(vertical,5,0,10))return fail("InvalidWings");
        for(size_t i=0;i<3;++i)out->wings[i]=stats.array[i].number;
        for(size_t i=0;i<5;++i)out->wings[i+3]=vertical.array[i].number;
        out->wing=true;
    }
    // Armor/accessory operations travel in separately validated, typed columns.
    // Never accept an old descriptor whose gameplay would otherwise disappear.
    if(root.at("armorSet").kind!=Json::Null&&!entry.armor.hasSet)return fail("MissingNativeArmorSet");
    const auto& armor=root.at("armor");
    if(armor.kind!=Json::Null&&(!armor.at("effects").array.empty()||!armor.at("immune").array.empty())&&!entry.armor.present)return fail("MissingNativeArmor");
    const auto& effects=root.at("effects");
    if(effects.kind!=Json::Array||effects.array.size()>16||effects.array.size()!=entry.accessoryEffects.size())return fail("MissingNativeAccessoryEffects");
    return true;
}
} // namespace item_policy_wire
} // namespace bl::csharp
