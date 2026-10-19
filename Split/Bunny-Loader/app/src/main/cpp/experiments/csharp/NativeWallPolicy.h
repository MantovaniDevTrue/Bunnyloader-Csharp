#pragma once
#include "experiments/csharp/BasicItemDefaults.h"
#include <array>
#include <algorithm>
#include <set>

namespace bl::csharp::walls {
struct MapEntry {int r=0,g=0,b=0;std::string name;};
struct Definition {
    std::string mod,name,source,blend,dust;
    int type=-1,failDust=1,normalDust=3,frames=1,ticks=5,from=0;
    bool housing=false;
    float chance=0;
    std::vector<int> choices;
    std::map<int,std::string> frameDust;
    std::array<float,3> day{},night{};
    std::vector<MapEntry> maps;
};
inline bool id(const std::string& text) {
    if(text.empty()||text.size()>72||!((text[0]>='A'&&text[0]<='Z')||(text[0]>='a'&&text[0]<='z')))return false;
    for(char c:text)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
    return true;
}
inline bool validUtf8Text(const std::string& text) {
    for(size_t i=0;i<text.size();) {
        const auto lead=static_cast<unsigned char>(text[i++]);uint32_t code=lead;int tail=0;uint32_t minimum=0;
        if(lead<0x80){if(lead<32||lead==127)return false;continue;}
        if(lead>=0xc2&&lead<=0xdf){tail=1;code=lead&31;minimum=0x80;}
        else if(lead>=0xe0&&lead<=0xef){tail=2;code=lead&15;minimum=0x800;}
        else if(lead>=0xf0&&lead<=0xf4){tail=3;code=lead&7;minimum=0x10000;}
        else return false;
        if(size_t(tail)>text.size()-i)return false;
        while(tail--){auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80)return false;code=(code<<6)|(next&63);}
        if(code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
    }
    return true;
}
inline bool parse(const std::vector<std::string>& f,Definition* out,std::string* error) {
    Definition d;
    const auto reject=[&](const char* field,const std::string& actual,const char* reason){
        *error="[CSharp][ERROR][WallDefaults] Class="+(f.size()>1?f[1]:"<unavailable>")+" Stage=Decode Field="+field+" Actual="+actual+" Result=Rejected Reason="+reason;return false;};
    if(f.size()!=16||f[0]!="WALL"||!id(f[1]))return reject("Wire",std::to_string(f.size()),"InvalidWallRecord");
    d.name=f[1];if(f[2]!="0"&&f[2]!="1")return reject("Housing",f[2],"ExpectedBoolean");d.housing=f[2]=="1";
    if(f[3]!="-"){if(!id(f[3]))return reject("BlendWallName",f[3],"InvalidNativeId");d.blend=f[3];}
    if(f[4]!="-"){if(!id(f[4]))return reject("DustName",f[4],"InvalidNativeId");d.dust=f[4];}
    int* ints[]={&d.failDust,&d.normalDust,&d.frames,&d.ticks,&d.from};
    const int maximum[]={100,100,256,255,2};const int minimum[]={0,0,1,1,0};
    const char* names[]={"FailedDustCount","DustCount","AnimationFrames","AnimationFrameTicks","FrameSource"};
    for(int n=0;n<5;++n)if(!basicInteger(f[5+n],ints[n])||*ints[n]<minimum[n]||*ints[n]>maximum[n])return reject(names[n],f[5+n],"OutOfRange");
    char* end=nullptr;d.chance=std::strtof(f[10].c_str(),&end);
    if(f[10].empty()||end!=f[10].c_str()+f[10].size()||!std::isfinite(d.chance)||d.chance<0||d.chance>1)return reject("FrameChance",f[10],"ExpectedFiniteProbability");
    if(f[11]!="-")for(auto& part:basicSplit(f[11],',')) {
        int value=-1;if(!basicInteger(part,&value)||value<0||value>2||d.choices.size()>=3||std::find(d.choices.begin(),d.choices.end(),value)!=d.choices.end())return reject("FrameChoices",f[11],"InvalidFrameChoice");d.choices.push_back(value);
    }
    if(d.chance>0&&d.choices.empty())return reject("FrameChoices",f[11],"MissingFrameChoices");
    if(f[12]!="-")for(auto& part:basicSplit(f[12],',')) {
        auto pair=basicSplit(part,':');int number=-1;
        if(pair.size()!=2||!basicInteger(pair[0],&number)||number<0||number>2||!id(pair[1])||!d.frameDust.emplace(number,pair[1]).second)return reject("FrameDust",f[12],"InvalidFrameDust");
    }
    for(int n=0;n<2;++n){auto rgb=basicSplit(f[13+n],',');if(rgb.size()!=3)return reject("Light",f[13+n],"ExpectedRgb");
        for(int c=0;c<3;++c){end=nullptr;float value=std::strtof(rgb[c].c_str(),&end);if(rgb[c].empty()||end!=rgb[c].c_str()+rgb[c].size()||!std::isfinite(value)||value<0||value>10)return reject("Light",f[13+n],"ExpectedFiniteRgb0To10");(n?d.night:d.day)[c]=value;}}
    if(f[15]!="-")for(auto& token:basicSplit(f[15],';')) {
        auto parts=basicSplit(token,',');MapEntry m;
        if(parts.size()!=4||!basicInteger(parts[0],&m.r)||!basicInteger(parts[1],&m.g)||!basicInteger(parts[2],&m.b)||m.r<0||m.r>255||m.g<0||m.g>255||m.b<0||m.b>255||parts[3].empty()||parts[3].size()>320||parts[3].size()%2||d.maps.size()>=16)return reject("Map",token,"InvalidMapEntry");
        for(size_t p=0;p<parts[3].size();p+=2){auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;};int a=hex(parts[3][p]),b=hex(parts[3][p+1]);if(a<0||b<0||((a<<4)|b)<32||((a<<4)|b)==127)return reject("MapName",parts[3],"InvalidUtf8Hex");m.name+=static_cast<char>((a<<4)|b);}
        if(!validUtf8Text(m.name))return reject("MapName",parts[3],"InvalidUtf8Text");
        d.maps.push_back(std::move(m));
    }
    *out=std::move(d);return true;
}
inline int nextFrame(int frame,int* counter,int frames,int ticks) {
    if(++*counter>=ticks){*counter=0;return (frame+1)%frames;}return frame;
}
}
