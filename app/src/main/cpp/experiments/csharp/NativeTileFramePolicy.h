#pragma once
#include <string_view>
namespace bl::csharp::tile_frame {
inline bool parseReply(std::string_view reply,bool* run,bool* reset,bool* noBreak) {
    if(!run||!reset||!noBreak||reply.size()!=11||reply.substr(0,6)!="FRAME|"||
       reply[7]!='|'||reply[9]!='|')return false;
    for(int i=6;i<=10;i+=2)if(reply[i]!='0'&&reply[i]!='1')return false;
    *run=reply[6]=='1';*reset=reply[8]=='1';*noBreak=reply[10]=='1';
    return true;
}
}
