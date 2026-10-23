#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp {

struct NativeTooltipLine {
    std::string name;
    std::string text;
    std::string color;
    bool draw=true;
    bool rainbow=false;
    float amplitude=3, speed=8, letterStep=1.15f, hueStep=0.04f, hueSpeed=0.20f;
};

struct NativeTooltipPlan {
    std::vector<NativeTooltipLine> lines;
};

struct NativeTooltipGlyph {
    float yOffset=0;
    uint8_t r=255,g=255,b=255;
};

inline NativeTooltipGlyph nativeTooltipGlyph(const NativeTooltipLine& line,float clock,size_t index) {
    NativeTooltipGlyph result;
    if(!line.rainbow || !std::isfinite(clock))return result;
    const float step=static_cast<float>(index);
    result.yOffset=std::sin(clock*line.speed+step*line.letterStep)*line.amplitude;
    float hue=clock*line.hueSpeed+step*line.hueStep;
    hue=hue-std::floor(hue);
    const float sector=hue*6.f;
    const int part=static_cast<int>(std::floor(sector));
    const float f=sector-static_cast<float>(part);
    const auto q=static_cast<uint8_t>((1.f-f)*255.f);
    const auto u=static_cast<uint8_t>(f*255.f);
    switch(part) {
    case 0:result.r=255;result.g=u;result.b=0;break;
    case 1:result.r=q;result.g=255;result.b=0;break;
    case 2:result.r=0;result.g=255;result.b=u;break;
    case 3:result.r=0;result.g=q;result.b=255;break;
    case 4:result.r=u;result.g=0;result.b=255;break;
    default:result.r=255;result.g=0;result.b=q;break;
    }
    return result;
}

class NativeTooltipDecoder {
    std::string_view wire;
    size_t at=0;
    static int nibble(char c) {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    }
    void space() {while(at<wire.size()&&(wire[at]==' '||wire[at]=='\n'||wire[at]=='\r'||wire[at]=='\t'))++at;}
    bool take(char c){space();if(at>=wire.size()||wire[at]!=c)return false;++at;return true;}
    bool literal(std::string_view value) {
        space();if(wire.substr(at,value.size())!=value)return false;
        at+=value.size();return true;
    }
    bool unicode(uint32_t* value) {
        if(wire.size()-at<4)return false;
        uint32_t n=0;
        for(int i=0;i<4;++i) {int d=nibble(wire[at++]);if(d<0)return false;n=(n<<4)|static_cast<uint32_t>(d);}
        *value=n;return true;
    }
    static void utf8(std::string* target,uint32_t value) {
        if(value<0x80)target->push_back(static_cast<char>(value));
        else if(value<0x800) {
            target->push_back(static_cast<char>(0xC0|(value>>6)));
            target->push_back(static_cast<char>(0x80|(value&63)));
        }else if(value<0x10000) {
            target->push_back(static_cast<char>(0xE0|(value>>12)));
            target->push_back(static_cast<char>(0x80|((value>>6)&63)));
            target->push_back(static_cast<char>(0x80|(value&63)));
        }else {
            target->push_back(static_cast<char>(0xF0|(value>>18)));
            target->push_back(static_cast<char>(0x80|((value>>12)&63)));
            target->push_back(static_cast<char>(0x80|((value>>6)&63)));
            target->push_back(static_cast<char>(0x80|(value&63)));
        }
    }
    bool string(std::string* result) {
        if(!take('"'))return false;
        std::string text;
        while(at<wire.size()) {
            unsigned char c=static_cast<unsigned char>(wire[at++]);
            if(c=='"'){*result=std::move(text);return true;}
            if(c<32||c==127)return false;
            if(c!='\\'){text.push_back(static_cast<char>(c));if(text.size()>512)return false;continue;}
            if(at==wire.size())return false;
            char escaped=wire[at++];
            if(escaped=='u') {
                uint32_t cp=0;
                if(!unicode(&cp))return false;
                if(cp>=0xD800&&cp<=0xDBFF) {
                    if(at+2>wire.size()||wire.substr(at,2)!="\\u")return false;
                    at+=2;uint32_t low=0;
                    if(!unicode(&low)||low<0xDC00||low>0xDFFF)return false;
                    cp=0x10000+((cp-0xD800)<<10)+(low-0xDC00);
                }else if(cp>=0xDC00&&cp<=0xDFFF)return false;
                if(cp==0||cp==127)return false;
                utf8(&text,cp);
            }else if(escaped=='"'||escaped=='\\'||escaped=='/')text.push_back(escaped);
            else if(escaped=='n')text.push_back('\n');
            else if(escaped=='r')text.push_back('\r');
            else if(escaped=='t')text.push_back('\t');
            else if(escaped=='b')text.push_back('\b');
            else if(escaped=='f')text.push_back('\f');
            else return false;
            if(text.size()>512)return false;
        }
        return false;
    }
    bool key(std::string_view name) {
        std::string got;
        return string(&got)&&got==name&&take(':');
    }
    bool boolean(bool* out) {
        if(literal("true")){*out=true;return true;}
        if(literal("false")){*out=false;return true;}
        return false;
    }
    bool real(float* out,float maximum) {
        space();const size_t start=at;
        if(at<wire.size()&&wire[at]=='-')++at;
        if(at==wire.size()||wire[at]<'0'||wire[at]>'9')return false;
        while(at<wire.size()&&wire[at]>='0'&&wire[at]<='9')++at;
        if(at<wire.size()&&wire[at]=='.') {
            ++at;const size_t digits=at;
            while(at<wire.size()&&wire[at]>='0'&&wire[at]<='9')++at;
            if(at==digits)return false;
        }
        if(at<wire.size()&&(wire[at]=='e'||wire[at]=='E')) {
            ++at;if(at<wire.size()&&(wire[at]=='+'||wire[at]=='-'))++at;
            const size_t digits=at;
            while(at<wire.size()&&wire[at]>='0'&&wire[at]<='9')++at;
            if(at==digits)return false;
        }
        const std::string digits(wire.substr(start,at-start));
        char* end=nullptr;const float value=std::strtof(digits.c_str(),&end);
        if(!end||*end||!std::isfinite(value)||value<0.f||value>maximum)return false;
        *out=value;return true;
    }
    bool line(NativeTooltipLine* out) {
        if(!take('{')||!key("name")||!string(&out->name)||
           !take(',')||!key("text")||!string(&out->text)||
           !take(',')||!key("color")||!string(&out->color)||
           !take(',')||!key("draw")||!boolean(&out->draw)||
           !take(',')||!key("rainbow")||!boolean(&out->rainbow)||
           !take(',')||!key("amplitude")||!real(&out->amplitude,12)||
           !take(',')||!key("speed")||!real(&out->speed,32)||
           !take(',')||!key("letterStep")||!real(&out->letterStep,6)||
           !take(',')||!key("hueStep")||!real(&out->hueStep,1)||
           !take(',')||!key("hueSpeed")||!real(&out->hueSpeed,5)||
           !take('}'))return false;
        if(out->name.empty()||out->name.size()>72)return false;
        for(char c:out->name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c=='_'))return false;
        if(!out->color.empty()) {
            if(out->color.size()!=6)return false;
            for(char c:out->color)if(nibble(c)<0)return false;
        }
        return true;
    }
public:
    explicit NativeTooltipDecoder(std::string_view text):wire(text){}
    bool decode(NativeTooltipPlan* result) {
        if(!result||wire.empty()||wire.size()>4096||!take('['))return false;
        NativeTooltipPlan staged;
        std::set<std::string> names;
        if(!take(']')) {
            do {
                NativeTooltipLine next;
                if(staged.lines.size()>=24||!line(&next)||!names.insert(next.name).second)return false;
                staged.lines.push_back(std::move(next));
                if(take(']'))break;
                if(!take(','))return false;
            }while(true);
        }
        space();if(at!=wire.size())return false;
        *result=std::move(staged);return true;
    }
};

inline bool decodeNativeTooltipPlan(std::string_view wire,NativeTooltipPlan* plan,std::string* error) {
    if(NativeTooltipDecoder(wire).decode(plan))return true;
    if(error)*error="NativeTooltipPlan: invalid typed JSON schema, bounds or animation";
    return false;
}

}
