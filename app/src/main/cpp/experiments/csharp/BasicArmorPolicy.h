#pragma once
#include <cstdint>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <vector>

namespace bl::csharp {
struct BasicArmorEffect { std::string field,operation; float value=0; };
struct BasicArmorEffects { std::vector<BasicArmorEffect> effects; std::vector<std::string> immune; };
struct BasicArmorPolicy {
    bool present=false,hasSet=false;
    BasicArmorEffects piece,bonus;
    std::string head,body,legs,text,textKey;
};
inline std::vector<std::string> armorSplit(const std::string& s,char separator) {
    std::vector<std::string> out;size_t start=0;
    for(size_t i=0;i<=s.size();++i)if(i==s.size()||s[i]==separator){out.push_back(s.substr(start,i-start));start=i+1;}
    return out;
}
inline bool armorName(const std::string& value,bool qualified=false) {
    if(value.empty()||value.size()>(qualified?160u:72u))return false;
    unsigned slash=0;
    for(size_t i=0;i<value.size();++i) {
        const char c=value[i];
        if(qualified&&c=='/') {if(++slash>1||i==0||i+1==value.size())return false;continue;}
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||(qualified&&c=='-')))return false;
    }
    return true;
}
inline bool armorEffectsWire(const std::string& wire,const std::string& immunity,BasicArmorEffects* out) {
    BasicArmorEffects parsed;
    if(wire!="-") {
        const auto tokens=armorSplit(wire,';');if(tokens.size()>16)return false;
        for(const auto& token:tokens) {
            const auto p=armorSplit(token,':');if(p.size()!=3||p[2].empty()||p[2].size()>32)return false;
            bool integer=false;
            for(const char* field:{"statManaMax2","maxMinions","statDefense","lifeRegen"})if(p[0]==field)integer=true;
            bool floating=false;
            for(const char* field:{"meleeDamage","rangedDamage","magicDamage","minionDamage","moveSpeed"})if(p[0]==field)floating=true;
            float value=0;
            if(integer) {
                int n=0;auto result=std::from_chars(p[2].data(),p[2].data()+p[2].size(),n);
                if(p[1]!="int"||result.ec!=std::errc{}||result.ptr!=p[2].data()+p[2].size()||n< -1000||n>1000)return false;
                value=static_cast<float>(n);
            } else {
                if(!floating||(p[1]!="add"&&p[1]!="mul")||p[2].find_first_not_of("0123456789.eE+-")!=std::string::npos)return false;
                char* end=nullptr;errno=0;value=std::strtof(p[2].c_str(),&end);
                if(errno||end!=p[2].c_str()+p[2].size()||!std::isfinite(value)||value< -10||value>10||(p[1]=="mul"&&value<0))return false;
            }
            parsed.effects.push_back({p[0],p[1],value});
        }
    }
    if(immunity!="-") {
        parsed.immune=armorSplit(immunity,',');if(parsed.immune.size()>32)return false;
        for(size_t i=0;i<parsed.immune.size();++i) {
            if(!armorName(parsed.immune[i]))return false;
            for(size_t j=0;j<i;++j)if(parsed.immune[i]==parsed.immune[j])return false;
        }
    }
    *out=std::move(parsed);return true;
}
inline bool validArmorText(const std::string& text) {
    size_t units=0;
    for(size_t i=0;i<text.size();) {
        const auto first=static_cast<unsigned char>(text[i++]);uint32_t cp=first;unsigned more=0;uint32_t min=0;
        if(first>=0xc2&&first<=0xdf){cp=first&31;more=1;min=0x80;}
        else if(first>=0xe0&&first<=0xef){cp=first&15;more=2;min=0x800;}
        else if(first>=0xf0&&first<=0xf4){cp=first&7;more=3;min=0x10000;}
        else if(first>=0x80)return false;
        if(i+more>text.size())return false;
        for(unsigned n=0;n<more;++n){const auto c=static_cast<unsigned char>(text[i++]);if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
        if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||cp<32||(cp>=127&&cp<=159))return false;
        units+=cp>0xffff?2:1;if(units>512)return false;
    }
    return units>0;
}
inline bool parseBasicArmorPolicy(const std::string& wire,BasicArmorPolicy* out) {
    if(wire=="-"){*out={};return true;}
    if(wire.size()>8192)return false;
    const auto p=armorSplit(wire,'|');
    const bool localized=p.size()==10&&p[0]=="A2";
    if(!localized&&(p.size()!=9||p[0]!="A1"))return false;
    BasicArmorPolicy parsed;parsed.present=true;
    if(!armorEffectsWire(p[1],p[2],&parsed.piece))return false;
    if(p[3]=="-") {
        if(localized)return false;
        for(size_t i=4;i<9;++i)if(p[i]!="-")return false;
    } else {
        if(!armorName(p[3],true)||!armorName(p[4],true)||!armorName(p[5],true)||p[6].empty()||p[6].size()>4096||p[6].size()%2)return false;
        auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
        for(size_t i=0;i<p[6].size();i+=2) {
            const int a=digit(p[6][i]),b=digit(p[6][i+1]);if(a<0||b<0)return false;
            const int value=(a<<4)|b;if(value<32||value==127)return false;
            parsed.text.push_back(static_cast<char>(value));
        }
        if(!validArmorText(parsed.text)||!armorEffectsWire(p[7],p[8],&parsed.bonus))return false;
        parsed.hasSet=true;parsed.head=p[3];parsed.body=p[4];parsed.legs=p[5];
        if(localized) {
            if(p[9].empty()||p[9].size()>160)return false;
            for(const auto& part:armorSplit(p[9],'.'))if(!armorName(part))return false;
            parsed.textKey=p[9];
        }
    }
    *out=std::move(parsed);return true;
}
}
