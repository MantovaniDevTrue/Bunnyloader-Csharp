#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace bl::csharp::mount_policy {

struct Frames {
    int count=0,delay=0,start=0;
};

struct Definition {
    int kind=0;
    std::string_view texture,buff,dust,balloon;
    int jumpHeight=0,heightBoost=0,flightTime=0,fatigue=0,totalFrames=0;
    int playerYOffset=0,xOffset=0,yOffset=0,headOffset=0,bodyFrame=0;
    int balloonCount=0,ringCount=0,ringRadius=0,runDustThreshold=0,ambientChance=0;
    float acceleration=0,jumpSpeed=0,fallDamage=0,runSpeed=0,dashSpeed=0;
    bool blockExtraJumps=false,constantJump=false,idleLoop=false,minecartDelegates=false;
    std::array<Frames,6> frames{};
};

inline bool integer(std::string_view text,int* out,int min,int max) {
    if(text.empty()||!out)return false;
    int result=0;
    auto parse=std::from_chars(text.data(),text.data()+text.size(),result);
    if(parse.ec!=std::errc{}||parse.ptr!=text.data()+text.size()||
       result<min||result>max)return false;
    *out=result;
    return true;
}

inline bool single(std::string_view text,float* out,float min,float max) {
    if(text.empty()||!out)return false;
    float result=0;
    auto parse=std::from_chars(text.data(),text.data()+text.size(),result);
    if(parse.ec!=std::errc{}||parse.ptr!=text.data()+text.size()||
       !std::isfinite(result)||result<min||result>max)return false;
    *out=result;
    return true;
}

inline bool flag(std::string_view text,bool* out) {
    if(text!="0"&&text!="1")return false;
    *out=text=="1";return true;
}

inline bool path(std::string_view value,bool optional=false) {
    if(optional&&value=="-")return true;
    if(value.empty()||value.size()>200||value.front()=='/'||
       value.back()=='/'||value.find("//")!=value.npos)return false;
    for(unsigned char c:value)
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='/'||c=='_'||c=='-'))return false;
    return true;
}

inline bool name(std::string_view value) {
    if(value.empty()||value.size()>72)return false;
    for(unsigned char c:value)
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='_'))return false;
    return true;
}

inline bool parseFrame(std::string_view wire,Frames* output,int totalFrames) {
    std::array<std::string_view,3> fields{};
    size_t start=0,count=0;
    for(size_t i=0;i<=wire.size();++i)
        if(i==wire.size()||wire[i]==',') {
            if(count==fields.size())return false;
            fields[count++]=wire.substr(start,i-start);
            start=i+1;
        }
    Frames next;
    if(count!=3||!integer(fields[0],&next.count,0,64)||
       !integer(fields[1],&next.delay,0,600)||
       !integer(fields[2],&next.start,0,63)||
       (next.count==0&&next.delay!=0)||(next.count>0&&next.delay==0)||
       next.start+next.count>totalFrames)return false;
    *output=next;return true;
}

inline bool parse(std::string_view wire,Definition* output) {
    if(!output||wire.size()>2048)return false;
    std::array<std::string_view,36> fields{};
    size_t start=0,count=0;
    for(size_t i=0;i<=wire.size();++i)
        if(i==wire.size()||wire[i]=='|') {
            if(count==fields.size())return false;
            fields[count++]=wire.substr(start,i-start);
            start=i+1;
        }
    if(count!=fields.size()||fields[0]!="M1"||
       !path(fields[2])||!name(fields[3])||!name(fields[4])||
       !path(fields[29],true))return false;
    Definition next;
    if(!integer(fields[1],&next.kind,0,1)||
       !integer(fields[5],&next.jumpHeight,0,100)||
       !single(fields[6],&next.acceleration,0,100)||
       !single(fields[7],&next.jumpSpeed,0,100)||
       !flag(fields[8],&next.blockExtraJumps)||
       !flag(fields[9],&next.constantJump)||
       !integer(fields[10],&next.heightBoost,0,200)||
       !single(fields[11],&next.fallDamage,0,100)||
       !single(fields[12],&next.runSpeed,0,100)||
       !single(fields[13],&next.dashSpeed,0,100)||
       !integer(fields[14],&next.flightTime,0,36000)||
       !integer(fields[15],&next.fatigue,0,36000)||
       !integer(fields[16],&next.totalFrames,1,64)||
       !integer(fields[17],&next.playerYOffset,-100,100)||
       !integer(fields[18],&next.xOffset,-100,100)||
       !integer(fields[19],&next.yOffset,-100,100)||
       !integer(fields[20],&next.headOffset,-100,100)||
       !integer(fields[21],&next.bodyFrame,0,next.totalFrames-1)||
       !flag(fields[28],&next.idleLoop)||
       !integer(fields[30],&next.balloonCount,0,8)||
       !integer(fields[31],&next.ringCount,0,128)||
       !integer(fields[32],&next.ringRadius,0,512)||
       !integer(fields[33],&next.runDustThreshold,0,100)||
       !integer(fields[34],&next.ambientChance,0,10000)||
       !flag(fields[35],&next.minecartDelegates))return false;
    for(size_t i=0;i<next.frames.size();++i)
        if(!parseFrame(fields[22+i],&next.frames[i],next.totalFrames))return false;
    if((next.balloonCount>0)!=(fields[29]!="-")||
       (next.kind==1&&(next.balloonCount!=0||next.ringCount!=0||!next.minecartDelegates))||
       (next.kind==0&&next.minecartDelegates))return false;
    next.texture=fields[2];next.buff=fields[3];
    next.dust=fields[4];next.balloon=fields[29];
    *output=next;return true;
}

}
