#pragma once
#include <string_view>
namespace bl::csharp::tile_kill {
inline bool parseReply(std::string_view s,bool* fail,bool* effect,bool* noItem) {
    if(!fail||!effect||!noItem||s.size()!=10||s.substr(0,5)!="KILL|"||
       s[6]!='|'||s[8]!='|')return false;
    for(int i=5;i<=9;i+=2)if(s[i]!='0'&&s[i]!='1')return false;
    *fail=s[5]=='1';*effect=s[7]=='1';*noItem=s[9]=='1';return true;
}
}
