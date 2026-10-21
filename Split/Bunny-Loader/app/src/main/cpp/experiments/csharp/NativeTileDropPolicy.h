#pragma once
#include <charconv>
#include <string>
#include <vector>
#include <algorithm>
#include <string_view>
namespace bl::csharp::tile_drop {
struct Rule {int style=-1,itemType=0;std::string itemName;};
inline bool parseRules(std::string_view wire,std::vector<Rule>* rules) {
    if(!rules)return false;
    std::vector<Rule> result;
    if(wire=="-"){*rules={};return true;}
    if(wire.empty()||wire.size()>1536)return false;
    size_t cursor=0;
    while(cursor<wire.size()) {
        const size_t end=wire.find(';',cursor);
        const auto part=wire.substr(cursor,end==std::string_view::npos?end: end-cursor);
        const size_t sep=part.find('~');
        if(sep==std::string_view::npos||part.find('~',sep+1)!=std::string_view::npos||
           result.size()>=16)return false;
        const auto number=part.substr(0,sep),name=part.substr(sep+1);
        int style=0;
        const auto parsed=std::from_chars(number.data(),number.data()+number.size(),style);
        if(number.empty()||parsed.ec!=std::errc{}||
           parsed.ptr!=number.data()+number.size()||style< -1||style>4095||
           std::any_of(result.begin(),result.end(),[&](const Rule& r){return r.style==style;})||
           name.empty()||name.size()>72)return false;
        Rule rule;rule.style=style;
        if(name.front()=='#') {
            const auto numeric=name.substr(1);
            if(numeric.empty()||numeric.size()>5)return false;
            const auto id=std::from_chars(numeric.data(),numeric.data()+numeric.size(),rule.itemType);
            if(id.ec!=std::errc{}||id.ptr!=numeric.data()+numeric.size()||
               rule.itemType<1||rule.itemType>65535)return false;
        } else {
            for(char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                                 (c>='0'&&c<='9')||c=='_'))return false;
            rule.itemName=std::string(name);
        }
        result.push_back(std::move(rule));
        if(end==std::string_view::npos)break;
        cursor=end+1;
        if(cursor==wire.size())return false;
    }
    *rules=std::move(result);
    return true;
}
struct Item {int type=0,stack=0;};
struct Drops {bool custom=false;Item first{},second{};};
inline bool readInt(std::string_view s,int lo,int hi,int* result) {
    if(!result||s.empty()||s.size()>5)return false;
    int n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||n<lo||n>hi)return false;
    *result=n;return true;
}
inline bool readItem(std::string_view s,Item* item) {
    if(!item)return false;
    const size_t comma=s.find(',');
    if(comma==std::string_view::npos||s.find(',',comma+1)!=std::string_view::npos)return false;
    Item local{};
    if(!readInt(s.substr(0,comma),1,65535,&local.type)||
       !readInt(s.substr(comma+1),1,9999,&local.stack))return false;
    *item=local;return true;
}
inline bool parseReply(std::string_view s,Drops* result) {
    if(!result)return false;
    if(s=="DEFAULT"){*result={};return true;}
    if(s.substr(0,6)!="DROPS|"||s.size()<7)return false;
    const char n=s[6];
    if(n<'0'||n>'2')return false;
    Drops temp{};temp.custom=true;
    if(n=='0'){if(s.size()!=7)return false;*result=temp;return true;}
    if(s.size()<10||s[7]!='|')return false;
    const auto rem=s.substr(8);
    const size_t separator=rem.find('|');
    if(n=='1') {
        if(separator!=std::string_view::npos||!readItem(rem,&temp.first))return false;
    } else {
        if(separator==std::string_view::npos||
           !readItem(rem.substr(0,separator),&temp.first)||
           !readItem(rem.substr(separator+1),&temp.second))return false;
    }
    *result=temp;return true;
}
}
